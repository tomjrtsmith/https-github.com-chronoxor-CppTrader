//
// Created by Ivan Shynkarenka on 04.08.2017
//

#include "catch.hpp"

#include "trader/providers/nasdaq/itch_handler.h"
#include "trader/market_manager.h"

#include "filesystem/file.h"

#include <algorithm>

using namespace CppCommon;
using namespace CppTrader;
using namespace CppTrader::ITCH;

namespace {

class MyMarketHandler : public MarketHandler
{
public:
    MyMarketHandler()
        : _updates(0),
          _symbols(0),
          _max_symbols(0),
          _order_books(0),
          _max_order_books(0),
          _orders(0),
          _max_orders(0),
          _add_order(0),
          _reduce_order(0),
          _modify_order(0),
          _replace_order(0),
          _delete_order(0),
          _execute_order(0)
    {}

    size_t updates() const { return _updates; }
    size_t max_symbols() const { return _max_symbols; }
    size_t max_order_books() const { return _max_order_books; }
    size_t max_orders() const { return _max_orders; }
    size_t add_order() const { return _add_order; }
    size_t reduce_order() const { return _reduce_order; }
    size_t modify_order() const { return _modify_order; }
    size_t replace_order() const { return _replace_order; }
    size_t delete_order() const { return _delete_order; }
    size_t execute_order() const { return _execute_order; }

protected:
    void onAddSymbol(const Symbol& symbol) override { ++_updates; ++_symbols; _max_symbols = std::max(_symbols, _max_symbols); }
    void onDeleteSymbol(const Symbol& symbol) override { ++_updates; --_symbols; }
    void onAddOrderBook(const OrderBook& order_book) override { ++_updates; ++_order_books; _max_order_books = std::max(_order_books, _max_order_books); }
    void onDeleteOrderBook(const OrderBook& order_book) override { ++_updates; --_order_books; }
    void onUpdateOrderBook(const OrderBook& order_book, const Level& level, bool top) override { ++_updates; }
    void onAddOrder(const Order& order) override { ++_updates; ++_orders; _max_orders = std::max(_orders, _max_orders); ++_add_order; }
    void onReduceOrder(const Order& order, uint64_t quantity) override { ++_updates; ++_reduce_order; }
    void onModifyOrder(const Order& order, uint64_t new_price, uint64_t new_quantity) override { ++_updates; ++_modify_order; }
    void onReplaceOrder(const Order& order, uint64_t new_id, uint64_t new_price, uint64_t new_quantity) override { ++_updates; ++_replace_order; }
    void onReplaceOrder(const Order& order, const Order& new_order) override { ++_updates; ++_replace_order; }
    void onUpdateOrder(const Order& order) override {}
    void onDeleteOrder(const Order& order) override { ++_updates; --_orders; ++_delete_order; }
    void onExecuteOrder(const Order& order, uint64_t price, uint64_t quantity) override { ++_updates; ++_execute_order; }

private:
    size_t _updates;
    size_t _symbols;
    size_t _max_symbols;
    size_t _order_books;
    size_t _max_order_books;
    size_t _orders;
    size_t _max_orders;
    size_t _add_order;
    size_t _reduce_order;
    size_t _modify_order;
    size_t _replace_order;
    size_t _delete_order;
    size_t _execute_order;
};

class MyITCHHandler : public ITCHHandler
{
public:
    MyITCHHandler(MarketManager& market)
        : _market(market),
          _messages(0),
          _errors(0)
    {}

    size_t messages() const { return _messages; }
    size_t errors() const { return _errors; }

protected:
    bool onMessage(const SystemEventMessage& message) override { ++_messages; return true; }
    bool onMessage(const StockDirectoryMessage& message) override { ++_messages; Symbol symbol(message.StockLocate, message.Stock); _market.AddSymbol(symbol); _market.AddOrderBook(symbol); return true; }
    bool onMessage(const StockTradingActionMessage& message) override { ++_messages; return true; }
    bool onMessage(const RegSHOMessage& message) override { ++_messages; return true; }
    bool onMessage(const MarketParticipantPositionMessage& message) override { ++_messages; return true; }
    bool onMessage(const MWCBDeclineMessage& message) override { ++_messages; return true; }
    bool onMessage(const MWCBStatusMessage& message) override { ++_messages; return true; }
    bool onMessage(const IPOQuotingMessage& message) override { ++_messages; return true; }
    bool onMessage(const AddOrderMessage& message) override { ++_messages; _market.AddOrder(Order(message.OrderReferenceNumber, message.StockLocate, OrderType::LIMIT, (message.BuySellIndicator == 'B') ? OrderSide::BUY : OrderSide::SELL, message.Price, message.Shares)); return true; }
    bool onMessage(const AddOrderMPIDMessage& message) override { ++_messages; _market.AddOrder(Order(message.OrderReferenceNumber, message.StockLocate, OrderType::LIMIT, (message.BuySellIndicator == 'B') ? OrderSide::BUY : OrderSide::SELL, message.Price, message.Shares)); return true; }
    bool onMessage(const OrderExecutedMessage& message) override { ++_messages; _market.ExecuteOrder(message.OrderReferenceNumber, message.ExecutedShares); return true; }
    bool onMessage(const OrderExecutedWithPriceMessage& message) override { ++_messages; _market.ExecuteOrder(message.OrderReferenceNumber, message.ExecutionPrice, message.ExecutedShares); return true; }
    bool onMessage(const OrderCancelMessage& message) override { ++_messages; _market.ReduceOrder(message.OrderReferenceNumber, message.CanceledShares); return true; }
    bool onMessage(const OrderDeleteMessage& message) override { ++_messages; _market.DeleteOrder(message.OrderReferenceNumber); return true; }
    bool onMessage(const OrderReplaceMessage& message) override { ++_messages; _market.ReplaceOrder(message.OriginalOrderReferenceNumber, message.NewOrderReferenceNumber, message.Price, message.Shares); return true; }
    bool onMessage(const TradeMessage& message) override { ++_messages; return true; }
    bool onMessage(const CrossTradeMessage& message) override { ++_messages; return true; }
    bool onMessage(const BrokenTradeMessage& message) override { ++_messages; return true; }
    bool onMessage(const NOIIMessage& message) override { ++_messages; return true; }
    bool onMessage(const RPIIMessage& message) override { ++_messages; return true; }
    bool onMessage(const UnknownMessage& message) override { ++_errors; return true; }

private:
    MarketManager& _market;
    size_t _messages;
    size_t _errors;
};

} // namespace

TEST_CASE("Market manager", "[CppTrader]")
{
    MyMarketHandler market_handler;
    MarketManager market(market_handler);
    MyITCHHandler itch_handler(market);

    // Open the input file
    File input("../../tools/itch/sample.itch");
    if (!input.IsExists())
        input = File("../tools/itch/sample.itch");
    REQUIRE(input.IsExists());
    input.Open(true, false);

    // Perform input
    size_t size;
    uint8_t buffer[8192];
    while ((size = input.Read(buffer, sizeof(buffer))) > 0)
    {
        // Process the buffer
        itch_handler.Process(buffer, size);
    }

    // Check results
    REQUIRE(market_handler.updates() == 226573);
    REQUIRE(itch_handler.messages() == 1563071);
    REQUIRE(itch_handler.errors() == 0);

    // Check market statistics
    REQUIRE(market_handler.max_symbols() == 8352);
    REQUIRE(market_handler.max_order_books() == 8352);
    REQUIRE(market_handler.max_orders() == 56245);

    // Check orders statistics
    REQUIRE(market_handler.add_order() == 58915);
    REQUIRE(market_handler.reduce_order() == 2435);
    REQUIRE(market_handler.modify_order() == 0);
    REQUIRE(market_handler.replace_order() == 0);
    REQUIRE(market_handler.delete_order() == 58915);
    REQUIRE(market_handler.execute_order() == 2435);
}