#include "external/boost/ut.hpp"
#include "external/nlohmann/json.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

using json = nlohmann::json;

// Because boost::ut has it's own concept of events, as does Game, we cannot just use do
// using namespace boost::ut; here. Instead, we alias it, and then use the alias inside the
// closure to make the user defined literals and all the other niceness available.
namespace ut = boost::ut;

namespace {
    // Give a region two productions of the same item, the way a terrain whose table names that item in
    // two of its slots does once both slots roll. The amounts are fixed because RANDOM_ECONOMY would
    // otherwise vary what Production's constructor hands back.
    void set_up_duplicate_iron(ARegion *region, int first_amount, int second_amount) {
        // Leave the silver productions alone; they carry the region's wages and entertainment.
        std::vector<Production *> keep;
        for (const auto p : region->products) {
            if (p->itemtype == I_SILVER) keep.push_back(p);
            else delete p;
        }
        region->products = keep;

        for (int amount : { first_amount, second_amount }) {
            Production *p = new Production(I_IRON, amount);
            p->amount = amount;
            p->baseamount = amount;
            region->products.push_back(p);
        }
    }

    // Order the unit to mine for a month and run the production phase.
    void mine_for_a_month(UnitTestHelper& helper, Faction *faction, Unit *unit) {
        std::stringstream ss;
        ss << "#atlantis " << faction->num << "\n";
        ss << "unit " << unit->num << "\n";
        ss << "produce iron\n";
        helper.parse_orders(faction->num, ss);
        helper.run_productions();
    }
}

// This suite will test various aspects of the Faction class in isolation.
ut::suite<"Produce"> produce_suite = []
{
  using namespace ut;

  "Producing an item with ORed inputs consumes personal items first"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();
    helper.enable(UnitTestHelper::Type::ITEM, I_FOOD, true);
    helper.enable(UnitTestHelper::Type::SKILL, S_COOKING, true);

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 10); // 10 men so we can produce up to 10 meals/month
    helper.set_skill_level(unit, S_COOKING, 1);
    unit->items.SetNum(I_LIVESTOCK, 5);

    // We have another unit which has grain.  Bug was the grain would be used before the personal items
    Unit *unit2 = helper.create_unit(faction, helper.get_region(0, 0, 0));
    unit2->SetFlag(1, 2);
    unit2->items.SetNum(I_LEADERS, 1);
    unit2->items.SetNum(I_GRAIN, 10);

    std::stringstream ss;
    ss << "#atlantis 3\n";
    ss << "unit 2\n";
    ss << "produce 3 meal\n";
    ss << "unit 3\n";
    ss << "share 1\n";
    helper.parse_orders(faction->num, ss);
    helper.run_productions();

    // Check the results after running the productions
    expect(unit->items.GetNum(I_FOOD) == 3_i);
    expect(unit->items.GetNum(I_LIVESTOCK) == 2_i); // 5 - 3 meals = 2 livestock left
    expect(unit2->items.GetNum(I_GRAIN) == 10_i); // Grain should not be consumed since we used personal items first
  };

  "Producing an item with ORed inputs consumes shared items if personal items are insufficient"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();
    helper.enable(UnitTestHelper::Type::ITEM, I_FOOD, true);
    helper.enable(UnitTestHelper::Type::SKILL, S_COOKING, true);

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 10); // 10 men so we can produce up to 10 meals/month
    helper.set_skill_level(unit, S_COOKING, 1);
    unit->items.SetNum(I_LIVESTOCK, 5);

    // We have another unit which has grain.  Bug was the grain would be used before the personal items
    Unit *unit2 = helper.create_unit(faction, helper.get_region(0, 0, 0));
    unit2->SetFlag(1, 2);
    unit2->items.SetNum(I_LEADERS, 1);
    unit2->items.SetNum(I_GRAIN, 10);

    std::stringstream ss;
    ss << "#atlantis 3\n";
    ss << "unit 2\n";
    ss << "produce 6 meal\n";
    ss << "unit 3\n";
    ss << "share 1\n";
    helper.parse_orders(faction->num, ss);
    helper.run_productions();

    // Check the results after running the productions
    expect(unit->items.GetNum(I_FOOD) == 6_i);
    expect(unit->items.GetNum(I_LIVESTOCK) == 0_i); // no livestock left
    expect(unit2->items.GetNum(I_GRAIN) == 9_i); // 1 grain used from shared.
  };

  // ModifyTerrainItems() can point two slots of one terrain at the same item, and since each slot rolls
  // separately a few regions then end up producing that item twice. The next two tests pin down both
  // halves of the fix: the second entry really is dead weight, and dropping it costs the region nothing.
  "A repeated production of the same item can never be harvested"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    Faction *faction = helper.create_faction("Test Faction");
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 5);
    helper.set_skill_level(unit, S_MINING, 1);

    ARegion *region = helper.get_region(0, 0, 0);
    set_up_duplicate_iron(region, 3, 5);

    mine_for_a_month(helper, faction, unit);

    // The unit works the first deposit dry and never touches the second: RunAProduction() deleted its
    // month order on the way out of the first pass, so nothing is left to work the second one.
    expect(unit->items.GetNum(I_IRON) == 3_i);
    expect(region->get_production_for_skill(I_IRON, S_MINING)->activity == 3_i);
    expect(region->products.back()->activity == 0_i);
  };

  "A region drops a repeated production and keeps harvesting the first"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    Faction *faction = helper.create_faction("Test Faction");
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_LEADERS, 5);
    helper.set_skill_level(unit, S_MINING, 1);

    ARegion *region = helper.get_region(0, 0, 0);
    set_up_duplicate_iron(region, 3, 5);

    // This is what SetupProds() does for a new world and Readin() for an existing one.
    expect(region->remove_duplicate_products() == 1_i);
    auto iron_entries = std::count_if(
        region->products.begin(),
        region->products.end(),
        [](Production *p) { return p->itemtype == I_IRON; }
    );
    expect(iron_entries == 1_l);

    // Two months of mining, with the between-turn reset of the deposit in between. The region yields
    // its first deposit each month, exactly as it did while the second entry was still sitting there.
    mine_for_a_month(helper, faction, unit);
    expect(unit->items.GetNum(I_IRON) == 3_i);

    region->UpdateProducts();
    mine_for_a_month(helper, faction, unit);
    expect(unit->items.GetNum(I_IRON) == 6_i);

    // And the player is told about one iron deposit rather than two.
    helper.setup_reports();
    json report;
    faction->build_json_report(report, &helper.game_object(), nullptr);
    auto products = report["regions"][0]["products"];
    auto iron_lines = std::count_if(products.begin(), products.end(), [](const json& item) {
        return item["tag"] == ItemDefs[I_IRON].abr;
    });
    expect(iron_lines == 1_l);
  };
};
