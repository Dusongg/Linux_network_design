#include <coroutine>
#include <iostream>
using namespace std;

class task {
private:
	//范围for
	struct promise_type;
	struct iterator {
		std::coroutine_handle<promise_type>& handle;
		iterator(auto h) : handle(h) {}
		int& operator*() {
			return handle.promise().value;
		}
		iterator operator++() {
			if (!handle.done()) handle.resume();
			return *this;
		}
		bool operator!=(const iterator&)const {
			return !handle.done();
		}
	};

public:
	struct promise_type {
		int value;
		auto get_return_object() {
			return task{ std::coroutine_handle<promise_type>::from_promise(*this) };
		}
		auto initial_suspend() noexcept { return std::suspend_always(); }
		auto final_suspend() noexcept { return std::suspend_always(); }
		std::suspend_always yield_value(int v) {value = v; return {}; }
		void return_void() {}
		void unhandled_exception() { std::terminate(); }
	};
private:
	std::coroutine_handle<promise_type> hd;
public:
	iterator begin() { return hd; }
	iterator end() { return hd; }

	explicit task(std::coroutine_handle<promise_type> h) : hd(h) {}

	bool resume() {
		if (!hd || hd.done()) { return false; }
		hd.resume();
		return !hd.done();
	}

	void destroy() {
		hd.destroy();
	}
};

task printNumber(int k) {
	for (int i = 0; i < k; i++) {
		// std::cout << std::format("number: {}\n", i);
		co_await std::suspend_always{};
	}
	
}

task iota(int value) {
	cout << "iota\n";
	while (value) {
		co_yield value;
		value--;
	}
}

int main() {
	for (auto x : iota(10)) {
		cout << x << endl;
	}
}
