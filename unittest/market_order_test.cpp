#include "external/boost/ut.hpp"

#include "game.h"
#include "gamedata.h"
#include "testhelper.hpp"

// Because boost::ut has it's own concept of events, as does Game, we cannot just use do
// using namespace boost::ut; here. Instead, we alias it, and then use the alias inside the
// closure to make the user defined literals and all the other niceness available.
namespace ut = boost::ut;

// This suite will test various aspects of the Faction class in isolation.
ut::suite<"Market Orders"> market_order_suite = []
{
  using namespace ut;

  "Multiple buy orders of same type are merged"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    unit->items.SetNum(I_SILVER, 8000);
    ARegion *region = unit->object->region;
    Unit *unit2 = helper.create_unit(faction, region);
    unit2->items.SetNum(I_LEADERS, 1);
    unit2->items.SetNum(I_SILVER, 8000);

    int item_id = -1;
    int max_amount = 0;
    int price = 0;
    for(const auto market : region->markets) {
      // M_SELL markets are what the region wants people to sell (ie, what it wants to buy).
      if (market->type == Market::MarketType::M_SELL) continue;
      // Don't buy men.
      if (ItemDefs[market->item].type & IT_MAN) continue;
      // Just for simplicity of testing, make the amount evenly divisible by 4.
      market->amount = market->amount ? (market->amount / 4) * 4 : 0;
      // Skip over things that don't have a quantity or price.
      if (market->amount == 0 || market->price == 0) continue;
      item_id = market->item;
      max_amount = market->amount;
      price = market->price;
      break;
    }

    expect(max_amount > 0); // silly, just make sure we have a market item that we found.

    std::stringstream ss;
    ss << "#atlantis 3\n";
    ss << "unit 2\n";
    for (int i = 0; i < max_amount; i++) {
      ss << "buy 1 " << ItemDefs[item_id].abr << std::endl;
      ss << "@buy 1 " << ItemDefs[item_id].abr << std::endl;
    }
    ss << "unit 3\n";
    ss << "buy " << std::to_string(max_amount) << ' ' << ItemDefs[item_id].abr << std::endl;
    ss << "@buy " << std::to_string(max_amount) << ' ' << ItemDefs[item_id].abr << std::endl;

    // each unit should end up with about 25% for each buy order.
    helper.parse_orders(faction->num, ss);
    helper.run_buy_orders();

    expect(unit->items.GetNum(item_id) == (max_amount / 2));
    expect(unit2->items.GetNum(item_id) == (max_amount / 2));

    expect(faction->errors.size() == 0_ul);
    expect(faction->events.size() == 4_ul);
    std::string amt_string = item_string(item_id, max_amount / 4);
    expect(faction->events[0].message == "Buys " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[0].unit == unit);
    expect(faction->events[1].message == "Buys " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[1].unit == unit);
    expect(faction->events[2].message == "Buys " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[2].unit == unit2);
    expect(faction->events[3].message == "Buys " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[3].unit == unit2);

    // Check the unit recurring orders.  There should be max_amount/2 recurring buy orders for each.
    expect(unit->oldorders.size() == static_cast<size_t>(max_amount));
    expect(unit2->oldorders.size() == 1_ul);
    // And make sure the orders are buy orders.
    expect(unit->oldorders.front() == "@buy 1 " + std::string(ItemDefs[item_id].abr));
    expect(unit->oldorders.back() == "@buy 1 " + std::string(ItemDefs[item_id].abr));
    expect(unit2->oldorders.front() == "@buy " + std::to_string(max_amount) + ' ' + std::string(ItemDefs[item_id].abr));
  };

  "Multiple sell orders of same type are merged"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *unit = helper.get_first_unit(faction);
    ARegion *region = unit->object->region;
    Unit *unit2 = helper.create_unit(faction, region);
    unit2->items.SetNum(I_LEADERS, 1);

    int item_id = -1;
    int max_amount = 0;
    int price = 0;
    for(const auto market : region->markets) {
      // M_BUY markets are what the region wants people to buy (ie, what it wants to sell).
      if (market->type == Market::MarketType::M_BUY) continue;
      // Don't sell men, we don't do slavery.
      if (ItemDefs[market->item].type & IT_MAN) continue;
      // Just for simplicity of testing, make the amount evenly divisible by 4.
      market->amount = market->amount ? (market->amount / 4) * 4 : 0;
      // Skip over things that don't have a quantity or price.
      if (market->amount == 0 || market->price == 0) continue;
      item_id = market->item;
      max_amount = market->amount;
      price = market->price;
      break;
    }

    expect(max_amount > 0); // silly, just make sure we have a market item that we found.

    // Make sure they have enough to sell the items.
    unit->items.SetNum(item_id, max_amount * 4);
    unit2->items.SetNum(item_id, max_amount * 4);

    std::stringstream ss;
    ss << "#atlantis 3\n";
    ss << "unit 2\n";
    for (int i = 0; i < max_amount; i++) {
      ss << "sell 1 " << ItemDefs[item_id].abr << std::endl;
      ss << "@sell 1 " << ItemDefs[item_id].abr << std::endl;
    }
    ss << "unit 3\n";
    ss << "sell " << std::to_string(max_amount) << ' ' << ItemDefs[item_id].abr << std::endl;
    ss << "@sell " << std::to_string(max_amount) << ' ' << ItemDefs[item_id].abr << std::endl;

    // each unit should end up with about 25% for each buy order.
    helper.parse_orders(faction->num, ss);
    helper.run_sell_orders();

    expect(unit->items.GetNum(item_id) == (max_amount * 4 - (max_amount / 2)));
    expect(unit->items.GetNum(I_SILVER) == (max_amount / 2) * price);
    expect(unit2->items.GetNum(item_id) == (max_amount * 4 - (max_amount / 2)));
    expect(unit2->items.GetNum(I_SILVER) == (max_amount / 2) * price);

    expect(faction->errors.size() == 0_ul);
    expect(faction->events.size() == 4_ul);
    std::string amt_string = item_string(item_id, max_amount / 4);
    expect(faction->events[0].message == "Sells " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[0].unit == unit);
    expect(faction->events[1].message == "Sells " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[1].unit == unit);
    expect(faction->events[2].message == "Sells " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[2].unit == unit2);
    expect(faction->events[3].message == "Sells " + amt_string + " at $" + std::to_string(price) + " each.");
    expect(faction->events[3].unit == unit2);

    // Check the unit recurring orders.  There should be max_amount/2 recurring buy orders for each.
    expect(unit->oldorders.size() == static_cast<size_t>(max_amount));
    expect(unit2->oldorders.size() == 1_ul);
    // And make sure the orders are buy orders.
    expect(unit->oldorders.front() == "@sell 1 " + std::string(ItemDefs[item_id].abr));
    expect(unit->oldorders.back() == "@sell 1 " + std::string(ItemDefs[item_id].abr));
    expect(unit2->oldorders.front() == "@sell " + std::to_string(max_amount) + ' ' + std::string(ItemDefs[item_id].abr));
  };

  "Recruiting into a unit which already holds recruited men"_test = []
  {
    UnitTestHelper helper;
    helper.initialize_game();
    helper.setup_turn();

    // Recruiting only seeds specialized skill experience in rulesets which require experience
    // (kingdoms is the only one that does), so switch that on for the duration of this test.
    int saved_experience = Globals->REQUIRED_EXPERIENCE;
    Globals->REQUIRED_EXPERIENCE = 50;

    std::string name = "Test Faction";
    Faction *faction = helper.create_faction(name);
    Unit *leader = helper.get_first_unit(faction);
    ARegion *region = leader->object->region;

    // Barbarians have four specialized skills. A race with more than one is required here: each of
    // them gets experience but no study days on recruit, and that is the state which used to make
    // the next recruit into the same unit segfault.
    int price = 10;
    Market *market = new Market(Market::MarketType::M_BUY, I_BARBARIAN, price, 200, 0, 10000, 0, 200);
    region->markets.push_back(market);

    // An empty unit, which is what FORM leaves behind.
    Unit *recruits = helper.create_unit(faction, region);
    recruits->items.SetNum(I_LEADERS, 0);
    recruits->items.SetNum(I_SILVER, 10000);

    std::stringstream first_buy;
    first_buy << "#atlantis " << faction->num << std::endl;
    first_buy << "unit " << recruits->num << std::endl;
    first_buy << "buy 70 " << ItemDefs[I_BARBARIAN].abr << std::endl;
    helper.parse_orders(faction->num, first_buy);
    helper.run_buy_orders();

    expect(recruits->items.GetNum(I_BARBARIAN) == 70_i);
    // Four skills, each holding experience and no study days at all.
    expect(recruits->skills.size() == 4_i);
    for (const auto skill : recruits->skills) {
      expect(skill->days == 0_ul);
      expect(skill->exp > 0_ul);
    }

    // Buying a single further man into that unit used to dereference a null pointer while diluting
    // the unit's skills, because no skill of it had any days to be the highest one.
    std::stringstream second_buy;
    second_buy << "#atlantis " << faction->num << std::endl;
    second_buy << "unit " << recruits->num << std::endl;
    second_buy << "buy 1 " << ItemDefs[I_BARBARIAN].abr << std::endl;
    helper.parse_orders(faction->num, second_buy);
    helper.run_buy_orders();

    expect(recruits->items.GetNum(I_BARBARIAN) == 71_i);
    expect(recruits->skills.size() == 4_i);
    expect(faction->errors.size() == 0_ul);

    Globals->REQUIRED_EXPERIENCE = saved_experience;
  };
};
