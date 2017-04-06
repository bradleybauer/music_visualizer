// http://codereview.stackexchange.com/questions/136297/constant-sized-circular-ring-buffer
#include <cstddef>
#include <utility>
#include <type_traits>
template <typename T, std::size_t mcapacity>
std::aligned_storage_t<mcapacity * sizeof(T), alignof( T )> data;
template <typename T, std::size_t mcapacity>
class circular_buffer {
	circular_buffer (const circular_buffer &) = delete;
	circular_buffer &operator=(const circular_buffer &) = delete;
	constexpr T* data() { return reinterpret_cast<T*>(&data); }
	constexpr T* const* data() const { return reinterpret_cast<T const*>(&data); }
	T* begin;
	T* end;
	std::size_t msize;
	bool is_empty;
	struct destructor {
		template <typename std::enable_if<!std::is_trivially_destructible<T>::value>::type* = nullptr>
		void destroy(T* object) {
			object->~T();
		}
		template <typename std::enable_if<std::is_trivially_destructible<T>::value>::type* = nullptr>
		void destroy(T*) = delete;
		void destroy_n(T* objects, std::size_t sz) {
			for (std::size_t i = 0; i < sz; ++i) {
				objects->~T();
				++objects;
			}
			::operator delete(objects);
		}
	};
	destructor destroyer;
  public:
	using value_type = T;
	using reference = value_type&;
	using const_reference = value_type const&;
	using pointer = value_type*;
	using const_pointer = value_type const*;
	using size_type = std::size_t;
	circular_buffer() noexcept :
	      begin(data()),
	      end(begin),
	      msize(0),
		  is_empty(false)
	{}
	void tidy() noexcept {
		++end;
		if (end == data() + mcapacity)
			end = data();
		if (msize != mcapacity)
			++msize;
	}
	template <typename... ArgTypes>
	void emplace(ArgTypes&&... args) {
		::new (end) T(std::forward<ArgTypes>(args)...);
		tidy();
	}
	void push(T const& value) {
		::new (end) T(value);
		tidy();
	}
	void push(T&& value) {
		::new (end) T(std::move(value));
		tidy();
	}
	const_reference front() const {
		return *begin;
	}
	void pop() {
		destroyer.destroy(begin);
		++begin;
		if (begin == data() + mcapacity) {
			begin = data();
		}
		if (msize != 0) {
			--msize;
		}
	}
	size_type size() const noexcept {
		return msize;
	}
	// constexpr specifier declares that it is possible to evaluate the value of the function or variable at compile time.
	// http://en.cppreference.com/w/cpp/language/constexpr
	constexpr size_type capacity() noexcept {
		return mcapacity;
	}
	bool full() const {
		return msize == mcapacity;
	}
	bool empty() const {
		return msize == 0;
	}
	~circular_buffer() {
		destroyer.destroy_n(data(), mcapacity);
	}
};
