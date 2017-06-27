#include <sys/mman.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <iostream>

namespace {
	void *addr = nullptr;
	const size_t PAGE_SIZE = 1 << 12; // 4096
	const size_t SIZE_PER_FUNC = (1 << 8) - 1; // 255

	void* allocate_page() {
		char* page = static_cast<char*>(mmap(nullptr, PAGE_SIZE, PROT_EXEC | PROT_READ |		
							PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		if (page == MAP_FAILED) {
			return nullptr;
		}
		*page = 0;
		page += sizeof(int);
		for (size_t i = 0; i < PAGE_SIZE - sizeof(int); i += SIZE_PER_FUNC) {
			*reinterpret_cast<char**>(page + i - (i == 0 ? 0 : SIZE_PER_FUNC)) = page + i;
		}
		return page;
	}
	bool counter_add(void *page, int delta) {
		void *start_of_page = (void*)(((size_t)page >> 12) << 12);
		*static_cast<int*>(start_of_page) += delta;
		if (*static_cast<int*>(start_of_page) == 0) {
			munmap(start_of_page, PAGE_SIZE);
			return false;
		}
		return true;
	}
}

void* my_malloc() {
	if (!addr) {
		addr = allocate_page();
		if (!addr) {
			return nullptr;
		}
	}
	counter_add(addr, 1);
	void *ret = addr;
	addr = *reinterpret_cast<void**>(addr);
	return ret;
}
void my_free(void *ptr) {

	*reinterpret_cast<void**>(ptr) = addr;
	if (counter_add(ptr, -1)) {
		addr = ptr;
	} else {
		addr = nullptr;
	}
}
