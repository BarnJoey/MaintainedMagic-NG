#pragma once

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <type_traits>

// Improved BiMap
// - GetForwardMap/GetReverseMap return const references (no copies)
// - getValue/getKey use find() to avoid double lookups
// - getValueOrNull/getKeyOrNull return std::optional to avoid UB when Key/Value are not pointers
// - added noexcept where appropriate

template <typename KeyType, typename ValueType>
class BiMap
{
public:
	using forward_map_t = std::map<KeyType, ValueType>;
	using reverse_map_t = std::map<ValueType, KeyType>;
	using size_type = typename forward_map_t::size_type;

private:
	forward_map_t forwardMap;
	reverse_map_t reverseMap;

public:
	// Return const references to avoid expensive copies
	const forward_map_t& GetForwardMap() const noexcept { return forwardMap; }
	const reverse_map_t& GetReverseMap() const noexcept { return reverseMap; }

	// Insert or replace mapping
	void insert(const KeyType& key, const ValueType& value)
	{
		forwardMap.insert_or_assign(key, value);
		reverseMap.insert_or_assign(value, key);
	}

	// Throws if not found
	const ValueType& getValue(const KeyType& key) const
	{
		auto it = forwardMap.find(key);
		if (it == forwardMap.end()) {
			throw std::out_of_range("Key not found");
		}
		return it->second;
	}

	// Returns optional (no UB if KeyType/ValueType are non-pointer)
	std::optional<ValueType> getValueOrNull(const KeyType& key) const
	{
		auto it = forwardMap.find(key);
		if (it == forwardMap.end())
			return std::nullopt;
		return it->second;
	}

	const KeyType& getKey(const ValueType& value) const
	{
		auto it = reverseMap.find(value);
		if (it == reverseMap.end()) {
			throw std::out_of_range("Value not found");
		}
		return it->second;
	}

	std::optional<KeyType> getKeyOrNull(const ValueType& value) const
	{
		auto it = reverseMap.find(value);
		if (it == reverseMap.end())
			return std::nullopt;
		return it->second;
	}

	bool containsKey(const KeyType& key) const noexcept { return forwardMap.find(key) != forwardMap.end(); }
	bool containsValue(const ValueType& value) const noexcept { return reverseMap.find(value) != reverseMap.end(); }

	void eraseKey(const KeyType& key)
	{
		auto it = forwardMap.find(key);
		if (it != forwardMap.end()) {
			// erase corresponding reverse entry
			reverseMap.erase(it->second);
			forwardMap.erase(it);
		}
	}

	void eraseValue(const ValueType& value)
	{
		auto it = reverseMap.find(value);
		if (it != reverseMap.end()) {
			forwardMap.erase(it->second);
			reverseMap.erase(it);
		}
	}

	void clear() noexcept
	{
		forwardMap.clear();
		reverseMap.clear();
	}

	size_type size() const noexcept { return forwardMap.size(); }
	bool empty() const noexcept { return forwardMap.empty(); }
};
