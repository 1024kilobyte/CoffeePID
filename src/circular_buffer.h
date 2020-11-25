#pragma once

template<typename T>  // T is circular_buffer type
class circular_buffer_iterator {
public:
	// Basic types
	typedef class circular_buffer_iterator<T>	self_type;
	typedef T                                   cbuf_type;
	typedef std::random_access_iterator_tag     iterator_category;
	typedef typename cbuf_type::value_type      value_type;
	typedef typename cbuf_type::size_type       size_type;
	typedef typename cbuf_type::pointer         pointer;
	typedef typename cbuf_type::const_pointer   const_pointer;
	typedef typename cbuf_type::reference       reference;
	typedef typename cbuf_type::const_reference const_reference;
	typedef typename cbuf_type::difference_type difference_type;
	
	circular_buffer_iterator(
		cbuf_type* cb, size_t start_pos)
		: _circular_buffer(cb), _pos(start_pos) {}

	value_type& operator*() {return (*_circular_buffer)[_pos]; }
	value_type* operator->() { return &(operator*()); }

	self_type& operator++() { _pos++; return *this; }
	self_type operator++(int) { self_type tmp(*this); ++(*this); return tmp; }
	self_type& operator--() { _pos--; return *this; }
	self_type operator--(int) { self_type tmp(*this); --(*this); return tmp; }
	
	self_type operator+(difference_type n) const { self_type tmp(*this); tmp._pos += n; return tmp; }
	self_type& operator+=(difference_type n) { _pos += n; return *this; }
	self_type operator-(difference_type n) const { self_type tmp(*this); tmp._pos -= n; return tmp; }
	self_type& operator-=(difference_type n) { _pos -= n; return *this; }
	difference_type operator-(const self_type& other) const { return _pos - other._pos; }
	bool operator==(const self_type& other) const { return _pos == other._pos; }
	bool operator!=(const self_type& other) const { return !(*this == other); }
	bool operator>(const self_type& other) const { return _pos > other._pos; }
	bool operator>=(const self_type& other) const { return _pos >= other._pos; }
	bool operator<(const self_type& other) const { return _pos < other._pos; }
	bool operator<=(const self_type& other) const { return _pos <= other._pos; }

protected:
	cbuf_type* _circular_buffer;
	size_t     _pos;
};

template <class T>
class circular_buffer {
public:
	// Basic types
	typedef class circular_buffer<T> self_type;
	typedef T			value_type;
	typedef T*			pointer;
	typedef const T*	const_pointer;
	typedef T&			reference;
	typedef const T&	const_reference;
	typedef size_t		size_type;
	typedef ptrdiff_t	difference_type;

	explicit circular_buffer(size_type capacity = 300)
		: _buf(new value_type[capacity]),
		_buf_size(capacity),
		_head(0), _tail(0),
		_contents_size(0) {
	}

	~circular_buffer() {
		delete[] _buf;
	}

	reference front() { return _buf[_head]; }
	reference back() { return _buf[_tail]; }
	const_reference front() const { return _buf[_head]; }
	const_reference back() const { return _buf[_tail]; }
			   
	void clear() {
		_tail = _head = _contents_size = 0;
	}

	void increment_tail() {
		_tail++;
		if (_tail == _buf_size) _tail = 0;
		if (_contents_size < _buf_size) _contents_size++;
	}

	void increment_head() {
		// precondition: !empty()
		_head++;
		if (_head == _buf_size) _head = 0;
		_contents_size--;
	}

	void push_back(const value_type &item) {
		if (_contents_size == _buf_size) increment_head();
		if (!_contents_size) _tail--;
		increment_tail();
		_buf[_tail] = item;
	}

	value_type pop_front() {
		if (empty()) { return value_type(); }
		value_type val = _buf[_head];
		increment_head();
		return val;
	}
	
	bool empty() const {
		return (_contents_size == 0);
	}

	bool full() const {
		return (_contents_size == _buf_size);
	}

	size_type capacity() const
	{
		return _buf_size;
	}

	size_type max_size() const {
		return size_type(-1) / sizeof(value_type);
	}	
	
	size_type size() const  {
		return _contents_size;
	}
	
	void fill(const_reference value) {
		clear();
		for (int i = 0; i < capacity(); i++) push_back(value);
	}

	value_type &operator[](size_type index) {
		//if(index >= size()) return value_type();
		index += _head;
		if(index >= _buf_size) index -= _buf_size;
		return _buf[index];
	}

	typedef circular_buffer_iterator<self_type> iterator;

	iterator begin() { return iterator(this, 0); }
	iterator end() { return iterator(this, size()); }

	uint8_t* getBuffPtr() {
		return reinterpret_cast<uint8_t*>(_buf);
	}
	size_type getHead() {
		return _head;
	}

protected:
    value_type* _buf;
private:
	size_type _tail;
	size_type _head;
	const size_type _buf_size;
	size_type _contents_size;
};

template<class T>
class MeanBuffer : public circular_buffer<T> {
public:
	explicit MeanBuffer(size_t size) : circular_buffer<T>(size)
	{
		_sum = 0;
		_sum2 = 0;
	}

	void push_back(const T& item) {
		_sum += item;
		_sum2 += item * item;
		if (circular_buffer<T>::full()) pop_front();
		circular_buffer<T>::push_back(item);
	}

	T pop_front() {
		if (circular_buffer<T>::empty()) { return T(); }
		T old = circular_buffer<T>::pop_front();
		_sum -= old;
		_sum2 -= old * old;
		return old;
	}

	void clear() {
		_sum = 0;
		_sum2 = 0;
		circular_buffer<T>::clear();
	}

	void fill(const T& value) {
		clear();
		for (int i = 0; i < circular_buffer<T>::capacity(); i++) push_back(value);
	}

	float getMean() {
		if (circular_buffer<T>::empty()) return 0;
		return _sum / circular_buffer<T>::size();
	}

	float getVariance() {
		float size = circular_buffer<T>::size();
		if (size <= 1) return 0;
		float mean = getMean();
		return (_sum2 - 2 * mean * _sum + size * mean * mean) / (size - 1);
	}

	float getStdDev() {
		return sqrt(getVariance());
	}

	float getCV() {
		float mean = getMean();
		if (mean == 0) return 0;
		return getStdDev() / mean;
	}
private:
	float _sum;
	float _sum2;
};

template<class T>
class Median3Buffer : public circular_buffer<T> {
public:
	explicit Median3Buffer() : circular_buffer<T>(3){}

	T getMedian() {
		T middle;
		T a = circular_buffer<T>::_buf[0];
		T b = circular_buffer<T>::_buf[1];
		T c = circular_buffer<T>::_buf[2];

		if ((a <= b) && (a <= c)) {
			middle = (b <= c) ? b : c;
		}
		else if ((b <= a) && (b <= c)) {
			middle = (a <= c) ? a : c;
		}
		else {
			middle = (a <= b) ? a : b;
		}
		return middle;
	}

};
