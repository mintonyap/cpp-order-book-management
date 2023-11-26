//#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <functional>
#include "price.h"

class L3Book {
 public:
    void ProcessAdd(int64_t order_id, bool is_buy, double price, int64_t qty);
    void ProcessReplace(int64_t order_id, bool is_buy, double price, int64_t qty);
    void ProcessDelete(int64_t order_id);
    void ProcessExec(int64_t order_id, int64_t exec_qty);

 public:
    template <typename F>
    void ForEachLevel(F&& f) const {
        // TODO
        auto bid_it = buy_side_.begin();
        auto ask_it = sell_side_.begin();

        // Iterate over the levels until one side reaches the end
        while (bid_it != buy_side_.end() && ask_it != sell_side_.end()) {
            const Level& bid_level = bid_it->second;
            const Level& ask_level = ask_it->second;

            // Update the type of bid_price
            Price bid_price_scaled = bid_it->first;
            double bid_price = static_cast<double>(bid_price_scaled);
            int64_t bid_qty = bid_level.qty;
            int64_t bid_count = bid_level.count;

            // Update the type of ask_price
            Price ask_price_scaled = ask_it->first;
            double ask_price = static_cast<double>(ask_price_scaled);
            int64_t ask_qty = ask_level.qty;
            int64_t ask_count = ask_level.count;

            // Invoke the function object 'f' with the retrieved variables
            bool continue_iteration = f(
                bid_price, bid_qty, bid_count, ask_price, ask_qty, ask_count);

            // Check if the iteration should be stopped based on the return value of 'f'
            if (!continue_iteration) {
                break;
            }

            // Move to the next level on each side
            ++bid_it;
            ++ask_it;
        }

        // Continue iterating over the remaining levels on the buy side if any
        for (; bid_it != buy_side_.end(); ++bid_it) {
            const Level& level = bid_it->second;
            // Update the type of bid_price
            Price bid_price_scaled = bid_it->first;
            double bid_price = static_cast<double>(bid_price_scaled);
            int64_t bid_qty = level.qty;
            int64_t bid_count = level.count;

            // Invoke the function object 'f' with the retrieved variables
            bool continue_iteration = f(bid_price, bid_qty, bid_count, 0.0, 0, 0);  // Assuming ask variables are not available

            // Check if the iteration should be stopped based on the return value of 'f'
            if (!continue_iteration) {
                break;
            }
        }

        // Continue iterating over the remaining levels on the sell side if any
        for (; ask_it != sell_side_.end(); ++ask_it) {
            const Level& level = ask_it->second;
            double ask_price = ask_it->first;
            int64_t ask_qty = level.qty;
            int64_t ask_count = level.count;

            // Invoke the function object 'f' with the retrieved variables
            bool continue_iteration = f(0.0, 0, 0, ask_price, ask_qty, ask_count);  // Assuming bid variables are not available

            // Check if the iteration should be stopped based on the return value of 'f'
            if (!continue_iteration) {
                break;
            }
        }

        // f could be either a lambda function or a functor with the following function signature
        // bool process_level ( double bid_price , int64_t bid_qty , int64_t bid_count ,
        // double ask_price , int64_t ask_qty , int64_t ask_count );
        // 1. For the above book snapshot , below is the sequence of events that should be delivered to f
        // f (100.0 , 10, 2, 101.0 , 11, 3);
        // f (99.0 , 9, 3, 102.0 , 11, 2);
        // f( 0.0 , 0, 0, 103.0 , 12, 3);
        //}
        // 2. NOTE : the return type of "f" is bool , and this function should consider f’s return value :
        // -> true : continue the iteration if there ’s any more level
        // -> false : break the loop and stop the iteration
        // with the above event sequence , if f(99.0 , 9, 3,
    }

    template <typename F>
    void ForEachOrder(bool is_buy, bool inner_to_outer, F&& f) const {
        // TODO
        using OrderDetails = std::vector<std::tuple<double, int64_t, int64_t, int64_t>>;

        OrderDetails order_details;

        if (is_buy) {
            if (inner_to_outer) {
                // Collect order details from buy side in inner to outer order (highest to lowest price)
                for (auto it = buy_side_.rbegin(); it != buy_side_.rend(); ++it) {
                    const Level& level = it->second;
                    // Update the type of price
                    Price price_scaled = it->first;
                    double price = static_cast<double>(price_scaled);
                    int64_t qty = level.qty;

                    for (const auto& order : level.orders) {
                        order_details.emplace_back(price, qty, order->qty, order->order_id);
                    }
                }
            } else {
                // Collect order details from buy side in outer to inner order (highest to lowest price)
                for (auto it = buy_side_.begin(); it != buy_side_.end(); ++it) {
                    const Level& level = it->second;
                    // Update the type of price
                    Price price_scaled = it->first;
                    double price = static_cast<double>(price_scaled);
                    int64_t qty = level.qty;

                    for (const auto& order : level.orders) {
                        order_details.emplace_back(price, qty, order->qty, order->order_id);
                    }
                }
            }
        } else {
            if (inner_to_outer) {
                // Collect order details from sell side in inner to outer order (highest to lowest price)
                for (auto it = sell_side_.rbegin(); it != sell_side_.rend(); ++it) {
                    const Level& level = it->second;
                    // Update the type of price
                    Price price_scaled = it->first;
                    double price = static_cast<double>(price_scaled);

                    int64_t qty = level.qty;

                    for (const auto& order : level.orders) {
                        order_details.emplace_back(price, qty, order->qty, order->order_id);
                    }
                }
            } else {
                // Collect order details from sell side in outer to inner order (highest to lowest price)
                for (auto it = sell_side_.begin(); it != sell_side_.end(); ++it) {
                    const Level& level = it->second;
                    // Update the type of price
                    Price price_scaled = it->first;
                    double price = static_cast<double>(price_scaled);
                    int64_t qty = level.qty;

                    for (const auto& order : level.orders) {
                        order_details.emplace_back(price, qty, order->qty, order->order_id);
                    }
                }
            }
        }

        // Manually arrange the order details in the desired order (highest to lowest price)
        std::reverse(order_details.begin(), order_details.end());

        // Invoke the function object 'f' with the arranged order details
        for (const auto& order : order_details) {
            bool continue_iteration = f(is_buy, std::get<0>(order), std::get<1>(order), std::get<2>(order), std::get<3>(order));

            if (!continue_iteration) {
                break;
            }
        }

        // # There are four columns :
        // # a. side : S for Sell side , B for Buy side
        // # b. price : price for the level
        // # c. qty : aggregated qty for the level
        // # d. orders : list of order_qty ( order_id ), "1(20) " means a order with qty =1 and order_id =20
        // side price qty orders
        // S 103.00 12 1(20) , 1(17) , 10(3)
        // S 102.00 11 7(1) , 4(7)
        // S 101.00 11 3(7) , 4(3) , 4(5)
        // B 100.00 10 2(12) , 8(5)
        // B 99.00 9 1(41) , 1(40) , 7(28)
        // # 1. levels are printed from the highest price to lowest price , i.e. outer levels to inner levels for sell
        // side and inner levels to outer levels for buy side
        // # 2. for the top buy level
        // # a) the price is 100.00
        // # b) the aggregated qty is 10
        // # c) there are two orders in that level , order with qty =2, order_id =12 and order with qty =8, order_id =5

        // - is_buy : the side of the book to iterate .
        // true - iterate buy side book ;
        // false - iterate sell side book
        // - inner_to_outer : whether to iterate from inner levels to outer levels or from outer levels
        // to inner levels . In "L3 view of the Book " printing , we first iterate the sell side with
        // inner_to_outer = false (i.e. higher price to lower price ), and then buy side with
        // inner_to_outer = true (i.e. higher price to lower price )
        // - f: could be either a lambda function or a functor with the following function signature
        // bool process_order ( bool is_buy , double level_price , int64_t level_qty ,
        // int64_t qty , int64_t order_id );
        // For the above book snapshot , below is the sequence of events that should be delivered to f
        // f(false , 103.00 , 12, 1, 20) ;
        // f(false , 103.00 , 12, 1, 17) ;
        // f(false , 103.00 , 12, 10, 3);
        // calls to orders with price = 102 skipped here
        // f(false , 101 , 11, 3, 7);
        // f(false , 101 , 11, 4, 3);
        // f(false , 101 , 11, 4, 5);
        // f(true , 100 , 10, 2, 12) ;
        // f(true , 100 , 10, 8, 5);
        // calls to orders with price = 99.0 skipped here
        //}
        // 2. NOTE : the return type of "f" is bool to control the iteration
        // true : continue the iteration if there ’s any more order
        // false : break the loop and stop the iteration
        // 3. HINT : there ’s a reversed iterator , i.e. rbegin () , rend () , with which you can iterate
        // the container in reversed order .
        // a) In the L3Book , the Order pointers are saved in the Level in reversed order , i.e. Order
        // added later is infront of orders added earlier . You could use the reversed iterator to
        // iterate the container .
        // b) In the L3Book , both Buy side and Sell side books are stored from inner levels to outer
        // levels . You could also use reversed iterator to iterate the book from outer levels to
        // inner levels .
    }

    // to uncross the book by removing any offensive orders from the given book side
    // - is_buy: true. If the sell side is newer, and any offensive orders on the buy side should be removed
    void UncrossBookSide(bool is_buy);

 private:
    struct Order;
    struct Level {
        using Orders = std::list<Order*>;

        Level(double price) 
            : price(price), qty(0), count(0) {}

        Orders orders;
        double price;
        int64_t qty;
        int64_t count;
    };
    struct Order {
        Order(int64_t order_id, bool is_buy, double price, int64_t qty)
            : order_id(order_id), is_buy(is_buy), price(price), qty(qty) {} 

        int64_t order_id;
        bool is_buy;
        double price;
        int64_t qty;

        Level::Orders::iterator pos;        
        Level* level;
    };

    using BuySide = std::map<Price, Level, std::greater<Price>>;
    using SellSide = std::map<Price, Level, std::less<Price>>;
    using Orders = std::unordered_map<int64_t, Order>;

 private:
    Orders orders_;
    BuySide buy_side_;
    SellSide sell_side_;
};

// the requirement, the ops time complexity should be less than O(N), i.e. either O(1) or O(logN)

// OrderBook Update operation
// - add order: find the level and append to the level end 
// - update order: 
//   1) if priority is unchanged, update the qty inplace
//   2) if priority is changed, remove(order) and add(order)
// - delete order: find the level and remove from the level 

// OrderBook Query operation
// - iterate the levels up to some provide level count, to get the price, qty, ocount for each level.
// 
