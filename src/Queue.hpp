/* This file is part of Patchage.
 * Copyright 2007-2020 David Robillard <d@drobilla.net>
 *
 * Patchage is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or any later version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Patchage.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PATCHAGE_QUEUE_HPP
#define PATCHAGE_QUEUE_HPP

#include <atomic>
#include <cassert>
#include <cstddef>

/** Realtime-safe single-reader single-writer queue */
template<typename T>
class Queue
{
public:
	/** @param size Size in number of elements */
	explicit Queue(size_t size);

	Queue(const Queue&) = delete;
	Queue& operator=(const Queue&) = delete;

	Queue(Queue&&) = delete;
	Queue& operator=(Queue&&) = delete;

	~Queue();

	// Any thread:

	inline size_t capacity() const { return _size - 1; }

	// Write thread:

	inline bool full() const;
	inline bool push(const T& elem);

	// Read thread:

	inline bool empty() const;
	inline T&   front() const;
	inline void pop();

private:
	std::atomic<size_t> _front;   ///< Index to front of queue
	std::atomic<size_t> _back;    ///< Index to back of queue (one past end)
	const size_t        _size;    ///< Size of `_objects` (at most _size-1)
	T* const            _objects; ///< Fixed array containing queued elements
};

template<typename T>
Queue<T>::Queue(size_t size)
    : _front(0)
    , _back(0)
    , _size(size + 1)
    , _objects(new T[_size])
{
	assert(size > 1);
}

template<typename T>
Queue<T>::~Queue()
{
	delete[] _objects;
}

/** Return whether or not the queue is empty.
 */
template<typename T>
inline bool
Queue<T>::empty() const
{
	return (_back.load() == _front.load());
}

/** Return whether or not the queue is full.
 */
template<typename T>
inline bool
Queue<T>::full() const
{
	return (((_front.load() - _back.load() + _size) % _size) == 1);
}

/** Return the element at the front of the queue without removing it
 */
template<typename T>
inline T&
Queue<T>::front() const
{
	return _objects[_front.load()];
}

/** Push an item onto the back of the Queue - realtime-safe, not thread-safe.
 *
 * @returns true if `elem` was successfully pushed onto the queue,
 * false otherwise (queue is full).
 */
template<typename T>
inline bool
Queue<T>::push(const T& elem)
{
	if (full()) {
		return false;
	}

	const unsigned back = _back.load();
	_objects[back]      = elem;
	_back               = (back + 1) % _size;
	return true;
}

/** Pop an item off the front of the queue - realtime-safe, not thread-safe.
 *
 * It is a fatal error to call pop() when the queue is empty.
 */
template<typename T>
inline void
Queue<T>::pop()
{
	assert(!empty());
	assert(_size > 0);

	_front = (_front.load() + 1) % (_size);
}

#endif // PATCHAGE_QUEUE_HPP
