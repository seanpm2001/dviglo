// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../common/primitive_types.h"

#include <iterator>

namespace dviglo
{

/// Random access iterator.
template <class T> struct RandomAccessIterator
{
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = i32;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    /// Construct.
    constexpr RandomAccessIterator() :
        ptr_(nullptr)
    {
    }

    /// Construct with an object pointer.
    explicit constexpr RandomAccessIterator(T* ptr) :
        ptr_(ptr)
    {
    }

    /// Point to the object.
    constexpr T* operator ->() const { return ptr_; }

    /// Dereference the object.
    constexpr T& operator *() const { return *ptr_; }

    /// Preincrement the pointer.
    constexpr RandomAccessIterator<T>& operator ++()
    {
        ++ptr_;
        return *this;
    }

    /// Postincrement the pointer.
    constexpr RandomAccessIterator<T> operator ++(int)
    {
        RandomAccessIterator<T> it = *this;
        ++ptr_;
        return it;
    }

    /// Predecrement the pointer.
    constexpr RandomAccessIterator<T>& operator --()
    {
        --ptr_;
        return *this;
    }

    /// Postdecrement the pointer.
    constexpr RandomAccessIterator<T> operator --(int)
    {
        RandomAccessIterator<T> it = *this;
        --ptr_;
        return it;
    }

    /// Add an offset to the pointer.
    constexpr RandomAccessIterator<T>& operator +=(int value)
    {
        ptr_ += value;
        return *this;
    }

    /// Subtract an offset from the pointer.
    constexpr RandomAccessIterator<T>& operator -=(int value)
    {
        ptr_ -= value;
        return *this;
    }

    /// Add an offset to the pointer.
    constexpr RandomAccessIterator<T> operator +(int value) const { return RandomAccessIterator<T>(ptr_ + value); }

    /// Subtract an offset from the pointer.
    constexpr RandomAccessIterator<T> operator -(int value) const { return RandomAccessIterator<T>(ptr_ - value); }

    /// Calculate offset to another iterator.
    constexpr int operator -(const RandomAccessIterator& rhs) const { return (int)(ptr_ - rhs.ptr_); }

    /// Test for equality with another iterator.
    constexpr bool operator ==(const RandomAccessIterator& rhs) const { return ptr_ == rhs.ptr_; }

    /// Test for inequality with another iterator.
    constexpr bool operator !=(const RandomAccessIterator& rhs) const { return ptr_ != rhs.ptr_; }

    /// Test for less than with another iterator.
    constexpr bool operator <(const RandomAccessIterator& rhs) const { return ptr_ < rhs.ptr_; }

    /// Test for greater than with another iterator.
    constexpr bool operator >(const RandomAccessIterator& rhs) const { return ptr_ > rhs.ptr_; }

    /// Test for less than or equal with another iterator.
    constexpr bool operator <=(const RandomAccessIterator& rhs) const { return ptr_ <= rhs.ptr_; }

    /// Test for greater than or equal with another iterator.
    constexpr bool operator >=(const RandomAccessIterator& rhs) const { return ptr_ >= rhs.ptr_; }

    /// Pointer.
    T* ptr_;
};

/// Random access const iterator.
template <class T> struct RandomAccessConstIterator
{
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = i32;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    /// Construct.
    constexpr RandomAccessConstIterator() :
        ptr_(0)
    {
    }

    /// Construct with an object pointer.
    constexpr explicit RandomAccessConstIterator(const T* ptr) :
        ptr_(ptr)
    {
    }

    /// Construct from a non-const iterator.
    constexpr RandomAccessConstIterator(const RandomAccessIterator<T>& rhs) :     // NOLINT(google-explicit-constructor)
        ptr_(rhs.ptr_)
    {
    }

    /// Assign from a non-const iterator.
    constexpr RandomAccessConstIterator<T>& operator =(const RandomAccessIterator<T>& rhs)
    {
        ptr_ = rhs.ptr_;
        return *this;
    }

    /// Point to the object.
    constexpr const T* operator ->() const { return ptr_; }

    /// Dereference the object.
    constexpr const T& operator *() const { return *ptr_; }

    /// Preincrement the pointer.
   constexpr RandomAccessConstIterator<T>& operator ++()
    {
        ++ptr_;
        return *this;
    }

    /// Postincrement the pointer.
    constexpr RandomAccessConstIterator<T> operator ++(int)
    {
        RandomAccessConstIterator<T> it = *this;
        ++ptr_;
        return it;
    }

    /// Predecrement the pointer.
    constexpr RandomAccessConstIterator<T>& operator --()
    {
        --ptr_;
        return *this;
    }

    /// Postdecrement the pointer.
    constexpr RandomAccessConstIterator<T> operator --(int)
    {
        RandomAccessConstIterator<T> it = *this;
        --ptr_;
        return it;
    }

    /// Add an offset to the pointer.
    constexpr RandomAccessConstIterator<T>& operator +=(int value)
    {
        ptr_ += value;
        return *this;
    }

    /// Subtract an offset from the pointer.
    constexpr RandomAccessConstIterator<T>& operator -=(int value)
    {
        ptr_ -= value;
        return *this;
    }

    /// Add an offset to the pointer.
    constexpr RandomAccessConstIterator<T> operator +(int value) const { return RandomAccessConstIterator<T>(ptr_ + value); }

    /// Subtract an offset from the pointer.
    constexpr RandomAccessConstIterator<T> operator -(int value) const { return RandomAccessConstIterator<T>(ptr_ - value); }

    /// Calculate offset to another iterator.
    constexpr int operator -(const RandomAccessConstIterator& rhs) const { return (int)(ptr_ - rhs.ptr_); }

    /// Test for equality with another iterator.
    constexpr bool operator ==(const RandomAccessConstIterator& rhs) const { return ptr_ == rhs.ptr_; }

    /// Test for inequality with another iterator.
    constexpr bool operator !=(const RandomAccessConstIterator& rhs) const { return ptr_ != rhs.ptr_; }

    /// Test for less than with another iterator.
    constexpr bool operator <(const RandomAccessConstIterator& rhs) const { return ptr_ < rhs.ptr_; }

    /// Test for greater than with another iterator.
    constexpr bool operator >(const RandomAccessConstIterator& rhs) const { return ptr_ > rhs.ptr_; }

    /// Test for less than or equal with another iterator.
    constexpr bool operator <=(const RandomAccessConstIterator& rhs) const { return ptr_ <= rhs.ptr_; }

    /// Test for greater than or equal with another iterator.
    constexpr bool operator >=(const RandomAccessConstIterator& rhs) const { return ptr_ >= rhs.ptr_; }

    /// Pointer.
    const T* ptr_;
};

}
