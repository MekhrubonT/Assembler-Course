#include "args_classes.hpp"
#include <cstdint>
#include <algorithm>

extern void* my_malloc();
extern void my_free(void*);

template <typename T>
struct trampoline
{
    template <typename F>
    trampoline(F func)
    {}
    
    ~trampoline();
    
    T* get() const;
};

// mov rsi rdi
char shift_vars[5][3] = {{'\x48', '\x89', '\xfe'},
// mov rdx rsi
{'\x48', '\x89', '\xf2'},
// mov rcx rdx
{'\x48', '\x89', '\xd1'},
// mov r8 rcx
{'\x49', '\x89', '\xc8'},
// mov r9 r8
{'\x4d', '\x89', '\xc1'}};




template<typename R, typename... Args>
struct trampoline<R(Args...)>
{	
	template<typename Func>
	trampoline(Func const &f) 
		: func_obj(new Func(f))
		, caller((void*)&do_call<Func>)
        , deleter(&do_delete<Func>)
 	{
 		code = my_malloc();
 		auto tcode = (char*)code;
 		if (args_classes<Args...>::INTEGER >= 6) {
  			int stack_size = 8 * (args_classes<Args...>::INTEGER - 6
 								+ std::max(0, args_classes<Args...>::SSE - 8));
 			add_inst('\x4c', '\x8b', '\x1c', '\x24');
 			// push r9
 			add_inst('\x41', '\x51');
 			for (int nd_sh = 5; nd_sh; --nd_sh) {
 				for (auto c : shift_vars[nd_sh - 1]) {
 					add_inst(c);
 				}
 			}
 			// mov rax,[rsp]
 			add_inst('\x48', '\x89', '\xe0');
			
			// add rax [stack_size + 8]
			add_inst('\x48', '\x05', stack_size + 8);
			// add rsp 8
			add_inst('\x48', '\x81', '\xc4', 8);	



			char* label1 = static_cast<char*>(code);
			// cmp rax rsp
			add_inst('\x48', '\x39', '\xe0');
			// je
			add_inst('\x74');

			char* label2 = static_cast<char*>(code);
			code = (char*)code + 1;
			// add rsp 8
			add_inst('\x48', '\x81', '\xc4', 8);
			// mov rdi [rsp]
			add_inst('\x48', '\x8b', '\x3c', '\x24');
			// mov [rsp - 8] rdi
			add_inst('\x48', '\x89', '\x7c', '\x24', '\xf8');
			// jmp
			add_inst('\xeb');

			add_inst((char)(label1 - (char*)code - 1));
			*label2 = (char*)code - label2 - 1;


			// mov [rsp] r11
			add_inst('\x4c', '\x89', '\x1c', '\x24');
			// sub rsp stack_sizz+8
			add_inst('\x48', '\x81', '\xec', stack_size + 8);
 			// mov rdi imm
 			add_inst('\x48', '\xbf', func_obj);
 			// mov rax imm
 			add_inst('\x48', '\xb8', caller);
 			// call rax
 			add_inst('\xff', '\xd0');
 			// pop r9
 			add_inst('\x41', '\x59');


 			// mov r11 [rsp + stack_size]
 			add_inst('\x4c', '\x8b', '\x9c', '\x24', stack_size);
 			// mov [rsp] r11
 			add_inst('\x4c', '\x89', '\x1c', '\x24');
 			// ret
 			add_inst('\xc3');

 		} else {
 			for (int nd_sh = args_classes<Args...>::INTEGER; nd_sh; --nd_sh) {
 				for (auto c : shift_vars[nd_sh - 1]) {
 					add_inst(c);
 				}
 			}
 			// mov rdi imm
 			add_inst('\x48', '\xbf', func_obj);
 			// mov rax imm
 			add_inst('\x48', '\xb8', caller);
 			// mov jmp rax
 			add_inst('\xff', '\xe0');
 		}
 		code = tcode;
	}

	~trampoline() {
		if (code != nullptr) {
			deleter(func_obj);
			my_free(code);
		}
	}

	trampoline(const trampoline& other) = delete;
	trampoline(trampoline&& other) : 
		func_obj(other.func_obj),
		code(other.code),
		caller(other.caller),
		deleter(other.deleter) {
		other.func_obj = nullptr;
		other.code = nullptr;
		other.caller = nullptr;
		other.deleter = nullptr;
	}
	trampoline& operator=(trampoline&& other) {
		func_obj = other.func_obj;
		code = other.code;
		caller = other.caller;
		deleter = other.deleter;
		other.func_obj = nullptr;
		other.code = nullptr;
		other.caller = nullptr;
		other.deleter = nullptr;
	}


	R(*get() const)(Args...) {
		return (R (*)(Args...))code;
	}
private:
	template <typename F>
    static R do_call(void* obj, Args... args)
    {
        return (*(F*)obj)(args...);
    }

	template <typename F>
    static void do_delete(void* obj)
    {
		delete static_cast<F*>(obj);
    }


    void add_inst() {}
    template<typename... T>
    void add_inst(char com, T... codes) {
    	*static_cast<char*>(code) = com;
 		code = static_cast<char*>(code) + 1;
   		add_inst(codes...);
    }
    template<typename... T>
    void add_inst(int com, T... codes) {
    	*static_cast<uint32_t*>(code) = com;
   		code = static_cast<char*>(code) + 4;
   		add_inst(codes...);
    }
    template<typename... T>
    void add_inst(void *com, T... codes) {
    	*static_cast<void**>(code) = com;
   		code = static_cast<char*>(code) + 8;
   		add_inst(codes...);
    }


	void* func_obj;
    void* code;
    void* caller;
    void (*deleter)(void*);

};

