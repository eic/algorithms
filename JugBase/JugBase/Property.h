#pragma once

#include <string>

namespace algorithms {

  template <class TYPE>
  class Property {
  public:
    using StorageType = TYPE;
    using ValueType = typename std::remove_reference<StorageType>::type;

  private:
    StorageType m_value;

  public:
    template <class OWNER>
    Property(OWNER* /* owner */, std::string /* name */, TYPE&& value)
    : m_value( std::forward<TYPE>( value ) ) {
    }

    const ValueType& value() const { return this->m_value; }
    ValueType&       value() { return const_cast<ValueType&>( (const ValueType&)*this ); }

  };

} // namespace algorithms
