#pragma once

#include <map>
#include <optional>
#include <stdexcept>

// A simple, header-only BiMap using std::map (ordered, stable iteration).
// - C++20 niceties: insert_or_assign, [[nodiscard]]
// - Optional getters to avoid UB for non-pointer types.

template <class K, class V>
class BiMap
{
public:
	using forward_map_t = std::map<K, V>;
	using reverse_map_t = std::map<V, K>;
	using size_type = typename forward_map_t::size_type;

private:
	forward_map_t f_;
	reverse_map_t r_;

public:
	[[nodiscard]] const forward_map_t& GetForwardMap() const noexcept { return f_; }
	[[nodiscard]] const reverse_map_t& GetReverseMap() const noexcept { return r_; }

	void insert(const K& k, const V& v)
	{
		f_.insert_or_assign(k, v);
		r_.insert_or_assign(v, k);
	}
	void eraseKey(const K& k)
	{
		if (auto it = f_.find(k); it != f_.end()) {
			r_.erase(it->second);
			f_.erase(it);
		}
	}
	void eraseValue(const V& v)
	{
		if (auto it = r_.find(v); it != r_.end()) {
			f_.erase(it->second);
			r_.erase(it);
		}
	}
	void clear() noexcept
	{
		f_.clear();
		r_.clear();
	}

	[[nodiscard]] bool containsKey(const K& k) const noexcept { return f_.find(k) != f_.end(); }
	[[nodiscard]] bool containsValue(const V& v) const noexcept { return r_.find(v) != r_.end(); }

	[[nodiscard]] const V& getValue(const K& k) const
	{
		if (auto it = f_.find(k); it != f_.end())
			return it->second;
		throw std::out_of_range("Key not found");
	}
	[[nodiscard]] const K& getKey(const V& v) const
	{
		if (auto it = r_.find(v); it != r_.end())
			return it->second;
		throw std::out_of_range("Value not found");
	}

	[[nodiscard]] std::optional<V> getValueOrNull(const K& k) const
	{
		if (auto it = f_.find(k); it != f_.end())
			return it->second;
		return std::nullopt;
	}
	[[nodiscard]] std::optional<K> getKeyOrNull(const V& v) const
	{
		if (auto it = r_.find(v); it != r_.end())
			return it->second;
		return std::nullopt;
	}

	[[nodiscard]] size_type size() const noexcept { return f_.size(); }
	[[nodiscard]] bool empty() const noexcept { return f_.empty(); }
};
