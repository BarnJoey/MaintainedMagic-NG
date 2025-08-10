#include <stdexcept>

template <typename KeyType, typename ValueType>
class BiMap
{
private:
	std::map<KeyType, ValueType> forwardMap;
	std::map<ValueType, KeyType> reverseMap;

public:
	const std::map<KeyType, ValueType>& GetForwardMap() const noexcept { return forwardMap; }
	const std::map<KeyType, ValueType>& GetReverseMap() const noexcept { return reverseMap; }

	void insert(KeyType key, ValueType value)
	{
		forwardMap.insert_or_assign(key, value);
		reverseMap.insert_or_assign(value, key);
	}

	ValueType getValue(KeyType key)
	{
		auto it = forwardMap.find(key);
		if (it == forwardMap.end())
			throw std::out_of_range("Out of range");
		return it->second;
	}
	ValueType getValueOrNull(KeyType key)
	{
		static_assert(std::is_pointer_v<KeyType>);
		auto it = forwardMap.find(key);
		if (it == forwardMap.end())
			return nullptr;
		return it->second;
	}

	KeyType getKey(ValueType value)
	{
		auto it = reverseMap.find(value);
		if (it == reverseMap.end())
			throw std::out_of_range("Out of range");
		return it->second;
	}

	KeyType getKeyOrNull(ValueType value)
	{
		static_assert(std::is_pointer_v<ValueType>);
		auto it = reverseMap.find(value);
		if (it == reverseMap.end())
			return nullptr;
		return it->second;
	}

	bool containsKey(KeyType key)
	{
		return forwardMap.contains(key);
	}

	bool containsValue(ValueType value)
	{
		return reverseMap.contains(value);
	}

	void eraseKey(KeyType key)
	{
		if (forwardMap.contains(key)) {
			reverseMap.erase(forwardMap[key]);
			forwardMap.erase(key);
		}
	}

	void eraseValue(ValueType value)
	{
		if (reverseMap.contains(value)) {
			forwardMap.erase(reverseMap[value]);
			reverseMap.erase(value);
		}
	}

	void clear()
	{
		forwardMap.clear();
		reverseMap.clear();
	}

	size_t size()
	{
		return forwardMap.size();
	}

	bool empty()
	{
		return forwardMap.empty();
	}
};
