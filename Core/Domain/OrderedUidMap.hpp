// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ost_domain
{

    // A map from a uid string to a value that remembers the order in which the entries were added, like the insertion-
    // ordered dict the Windows page-folder tree is built in (hierarchy_reader.py: roots and subfolders appear in the
    // order the rows were read). It offers the lookups of the std::unordered_map it replaces (find, count, at,
    // operator[], size, empty, iteration over pair<uid, value>), but iterating yields the entries in insertion order.
    //
    // Lookup is linear: the maps hold the page folders of one bid (tens of entries). The value type may be incomplete
    // when OrderedUidMap<Value> is a member of Value itself (a folder holding its subfolders).
    template <class Value>
    class OrderedUidMap
    {
    public:
        using value_type = std::pair<std::string, Value>;
        using iterator = typename std::vector<value_type>::iterator;
        using const_iterator = typename std::vector<value_type>::const_iterator;

        iterator begin() { return _items.begin(); }
        iterator end() { return _items.end(); }
        const_iterator begin() const { return _items.begin(); }
        const_iterator end() const { return _items.end(); }
        const_iterator cbegin() const { return _items.begin(); }
        const_iterator cend() const { return _items.end(); }

        bool empty() const { return _items.empty(); }
        std::size_t size() const { return _items.size(); }
        void clear() { _items.clear(); }

        iterator find(const std::string &uid)
        {
            for (iterator it = _items.begin(); it != _items.end(); ++it)
                if (it->first == uid)
                    return it;
            return _items.end();
        }

        const_iterator find(const std::string &uid) const
        {
            for (const_iterator it = _items.begin(); it != _items.end(); ++it)
                if (it->first == uid)
                    return it;
            return _items.end();
        }

        std::size_t count(const std::string &uid) const { return find(uid) != _items.end() ? 1 : 0; }

        Value &at(const std::string &uid)
        {
            iterator it = find(uid);
            if (it == _items.end())
                throw std::out_of_range("OrderedUidMap::at: no entry for uid " + uid);
            return it->second;
        }

        const Value &at(const std::string &uid) const
        {
            const_iterator it = find(uid);
            if (it == _items.end())
                throw std::out_of_range("OrderedUidMap::at: no entry for uid " + uid);
            return it->second;
        }

        // The entry of `uid`; a default-constructed value is appended (last) when there is none.
        Value &operator[](const std::string &uid)
        {
            iterator it = find(uid);
            if (it != _items.end())
                return it->second;
            _items.emplace_back(uid, Value());
            return _items.back().second;
        }

        // Appends the entry; an existing entry of the same uid is left alone (std::unordered_map::emplace semantics).
        // Returns the entry and whether it was added.
        std::pair<iterator, bool> emplace(const std::string &uid, Value value)
        {
            iterator it = find(uid);
            if (it != _items.end())
                return {it, false};
            _items.emplace_back(uid, std::move(value));
            return {_items.end() - 1, true};
        }

    private:
        std::vector<value_type> _items;
    };

}
