#include <bits/stdc++.h>
#include <functional>

int greedy(const std::string& s) {
	int ans = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] != ' ' && (i == 0 || s[i - 1] == ' ')) {
			ans++;
		}
	}
	return ans;
}

void print8(__m128i &mask_of_spaces) {
	for (size_t i = 0; i < 16; ++i) {
		std::cout << (int)*((uint8_t*)&mask_of_spaces+i) << '\t';
	}
	std::cout << "\n";
}



int counter_sse(const std::string& s) {
	
	static __m128i spaces = _mm_set_epi8(' ', ' ', ' ', ' ', 
											' ', ' ', ' ', ' ', 
											' ', ' ', ' ', ' ', 
											' ', ' ', ' ', ' ');
	static __m128i ones = _mm_set_epi8(1, 1, 1, 1,
									   1, 1, 1, 1,
									   1, 1, 1, 1,
									   1, 1, 1, 1);
	__m128i bucket_ans = _mm_setzero_si128();
 	

 	int cur = 0;
 	int ans = 0;
 	bool is_space = true;
 	const char *data = s.c_str();
 	const auto handle = [&s, &cur, &is_space, &ans](const std::function<bool()> &predicat) {
	 	while (predicat() && cur < (int)s.size()) {
	 		if (s[cur] == ' ' && !is_space) {
	 			ans++;
	 		}
	 		is_space = (s[cur] == ' ');
	 		cur++; 	
	 	}
	};
	handle([&cur, &data]() { return cur == 0 || (size_t)(data + cur) % 16 != 0; });
 	const auto flush_ans = [&ans, &bucket_ans]() {
		for (size_t bucket = 0; bucket < 16; ++bucket) {
	 		ans += (int)*((uint8_t*)&bucket_ans + bucket);
	 	}
	 	bucket_ans = _mm_setzero_si128();
 	};

 	while (cur <= (int)s.size() - 16) {
 		__m128i mask_of_spaces = _mm_cmpeq_epi8(_mm_load_si128((__m128i*)(data + cur)), spaces);
 		__m128i mask_of_spaces_shifted = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i*)(data + cur - 1)), spaces);
 		__m128i temp = _mm_and_si128(_mm_andnot_si128(mask_of_spaces_shifted, mask_of_spaces), ones);
 		bucket_ans = _mm_add_epi8(bucket_ans, temp);
 		cur += 16;

 		static int counter = 0;
 		counter++;
 		if (counter == 255) {
 			flush_ans();
 			counter = 0;
 		}
 	}
 	flush_ans();
 	is_space = cur == 0 ? true : s[cur - 1] == ' ';
 	handle([]() {return true;});
 	return ans + !is_space;
}

std::string gen_random() {
	std::string s;
	int len = 100000;
    static const char alphanum[] =
        " A";

    for (int i = 0; i < len; ++i) {
        s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}

int main() {
	srand(time(0));
	for (int t = 0; t != 1e+3; ++t) {
		std::string s = gen_random();
		// std::cout << greedy(s) << "\n";
		// std::cout << counter_sse(s) << "\n";
		assert(greedy(s) == counter_sse(s));
	}
}