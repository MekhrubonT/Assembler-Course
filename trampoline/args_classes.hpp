
template<typename... Args>
struct args_classes;

template <>
struct args_classes<>
{
	static constexpr int INTEGER = 0;
	static constexpr int SSE = 0;
};

template <typename... Args>
struct args_classes<double, Args...>
{
	static constexpr int INTEGER = args_classes<Args...>::INTEGER;
	static constexpr int SSE = args_classes<Args...>::SSE + 1;
};
template <typename... Args>
struct args_classes<float, Args...>
{
	static constexpr int INTEGER = args_classes<Args...>::INTEGER;
	static constexpr int SSE = args_classes<Args...>::SSE + 1;
};
template <typename Any, typename... Args> 
struct args_classes<Any, Args...> 
{
	static constexpr int INTEGER = args_classes<Args...>::INTEGER + 1;
	static constexpr int SSE = args_classes<Args...>::SSE;
};

