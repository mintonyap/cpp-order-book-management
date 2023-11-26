#include <stdexcept>
#include "l3_book.h"

void L3Book::UncrossBookSide(bool is_buy) {
    // TODO
    if (is_buy) {
        if (!buy_side_.empty()) {
            double buy_price = static_cast<double>(buy_side_.begin()->first);
            auto sell_it = sell_side_.begin();
            while (sell_it != sell_side_.end()) {
                double sell_price = static_cast<double>(sell_it->first);
                if (sell_price <= buy_price) {
                    sell_it = sell_side_.erase(sell_it);
                } else {
                    ++sell_it;
                }
            }
        }
    } else {
        if (!sell_side_.empty()) {
            double sell_price = static_cast<double>(sell_side_.begin()->first);
            auto buy_it = buy_side_.begin();
            while (buy_it != buy_side_.end()) {
                double buy_price = static_cast<double>(buy_it->first);
                if (buy_price >= sell_price) {
                    buy_it = buy_side_.erase(buy_it);
                } else {
                    ++buy_it;
                }
            }
        }
    }
}

void L3Book::ProcessAdd(int64_t order_id, bool is_buy, double price, int64_t qty) {
    auto add_to_side = [=](auto& side) {
        // find or create the price level  
        const auto [level_itr, new_level] = side.emplace(Price(price), Level(price));
        Level* level = &(level_itr->second);

        // save the order to orders_
        const auto [order_itr, new_order] = orders_.emplace(
            order_id, Order(order_id, is_buy, price, qty)
        );
        if (!new_order) {
            throw std::runtime_error("add order with duplicated order_id");
        }
        Order* order = &(order_itr->second);

        // update the level's summary data
        level->qty += qty;
        level->count += 1;

        // link Level with Order, note:
        // 1. list::push_front: O(1) operation while push_back is O(N)
        // 2. iterator invalidatio rule: 
        //       a) reference to element in std::unordered_map is stable, i.e. level
        //       b) reference to element in std::list is stable, i.e. order
        level->orders.push_front(&order_itr->second);  
        order->level = level;
        order->pos = level->orders.begin();
    };

    if (is_buy) {
        add_to_side(buy_side_);
    } else {
        add_to_side(sell_side_);
    }
    UncrossBookSide(is_buy);
}

void L3Book::ProcessReplace(int64_t order_id, bool is_buy, double price, int64_t qty) {
    auto order_itr = orders_.find(order_id);
    if (order_itr == orders_.end()) {
        throw std::runtime_error("replace non-existing order");
    }
    auto& orig_order = order_itr->second;
    const bool inplace = Price(price) == Price(orig_order.price) && qty < orig_order.qty;
    if (inplace) {
        orig_order.level->qty -= (orig_order.qty - qty);
        orig_order.qty = qty;
    } else {
        ProcessDelete(order_id);
        ProcessAdd(order_id, is_buy, price, qty);
    }
}

void L3Book::ProcessDelete(int64_t order_id) {
    auto order_itr = orders_.find(order_id);
    if (order_itr == orders_.end()) {
        throw std::runtime_error("delete non-existing order");
    }
    const Order& order = order_itr->second;
    Level* level = order.level;

    // unlink Level with Order
    level->orders.erase(order.pos);

    // update the Level summary data
    level->qty -= order.qty;
    level->count -= 1;
    if (level->qty < 0 || level->count < 0) {
        throw std::runtime_error("delete more qty than available");
    }

    // delete the Level if no more orders
    auto delete_level = [] (auto& side, double price) {
        auto removed = side.erase(Price(price));
        if (removed == 0) {
            throw std::runtime_error("delete non-existing level");
        }
    };
    if (level->qty == 0) {
        if (order.is_buy) {
            delete_level(buy_side_, order.price);
        } else {
            delete_level(sell_side_, order.price);
        }
    }

    // delete the order
    orders_.erase(order_itr);
}

void L3Book::ProcessExec(int64_t order_id, int64_t exec_qty) {
    // TODO
    auto order_itr = orders_.find(order_id);
    if (order_itr == orders_.end()) {
        throw std::runtime_error("execute non-existing order");
    }
    auto& order = order_itr->second;

    if (exec_qty < order.qty) {
        // Partial execution
        order.qty -= exec_qty;
        order.level->qty -= exec_qty;
    } else {
        // Full execution (Delete operation)
        ProcessDelete(order_id);
    }
    UncrossBookSide(order.is_buy);
}
