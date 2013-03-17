/* This file is part of Patchage.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
 *
 * Patchage is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Patchage.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QUEUE_HPP_INCLUDED
#define QUEUE_HPP_INCLUDED

#include <cassert>

#ifdef __APPLE__
#    include <libkern/OSAtomic.h>
#endif

/**
   Realtime-safe single-reader single-writer queue.

   This is a RingBuffer but templated for fixed sized objects which makes its
   use a bit more efficient and cleaner in C++ than a traditional byte oriented
   ring buffer.
*/
template <typename T>
class Queue
{
public:
	/** @param size Size in number of elements */
	explicit Queue(uint32_t size);
	~Queue();

	// Any thread:

	inline uint32_t capacity() const { return _size - 1; }

	// Write thread(s):

	inline bool full() const;
	inline bool push(const T& obj);

	// Read thread:

	inline bool empty() const;
	inline T&   front() const;
	inline void pop();

private:
	Queue(const Queue&);             ///< Undefined (noncopyable)
	Queue& operator=(const Queue&);  ///< Undefined (noncopyable)

	static inline void barrier() {
#if defined(__APPLE__)
		OSMemoryBarrier();
#elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
		__sync_synchronize();
#else
#    warning Memory barriers unsupported, possible bugs on SMP systems
#endif
	}

	int            _front;    ///< Index to front of queue
	int            _back;     ///< Index to back of queue (1 past last element)
	const uint32_t _size;     ///< Size of _objects (1 more than can be stored)
	T* const       _objects;  ///< Circular array containing queued elements
};

template<typename T>
Queue<T>::Queue(uint32_t size)
	: _front(0)
	, _back(0)
	, _size(size + 1)
	, _objects(new T[_size])
{
	assert(size > 1);
}

template <typename T>
Queue<T>::~Queue()
{
	delete[] _objects;
}

/** Return whether or not the queue is empty.
 */
template <typename T>
inline bool
Queue<T>::empty() const
{
	return (_back == _front);
}

/** Return whether or not the queue is full.
 */
template <typename T>
inline bool
Queue<T>::full() const
{
	return (((_front - _back + _size) % _size) == 1);
}

/** Return the element at the front of the queue without removing it
 */
template <typename T>
inline T&
Queue<T>::front() const
{
	return _objects[_front];
}

/** Push an item onto the back of the Queue - realtime-safe, not thread-safe.
 *
 * @returns true if @a elem was successfully pushed onto the queue,
 * false otherwise (queue is full).
 */
template <typename T>
inline bool
Queue<T>::push(const T& elem)
{
	if (full()) {
		return false;
	} else {
		const int back = _back;
		_objects[back] = elem;
		barrier();
		_back = (back + 1) % _size;
		return true;
	}
}

/** Pop an item off the front of the queue - realtime-safe, not thread-safe.
 *
 * It is a fatal error to call pop() when the queue is empty.
 *
 * @returns the element popped.
 */
template <typename T>
inline void
Queue<T>::pop()
{
	assert(!empty());
	assert(_size > 0);

	barrier();
	_front = (_front + 1) % (_size);
}

#endif  // QUEUE_HPP_INCLUDED
