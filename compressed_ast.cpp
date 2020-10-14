// compressed_ast.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <stdint.h>
#include <atomic>
#include <cstring>
#include <map>

struct ast_t{
	using basic_tag_t = unsigned short; 
	char* data_ = nullptr;
	char* type_info_ = nullptr;

	std::size_t data_size_ = 0;
	std::size_t type_size_ = 0;

	std::size_t current_data_beg_ = 0;
	std::size_t current_type_beg_ = 0;
	std::size_t current_type_len_ = 0;

	std::size_t type_info_cur_ = 0;

	std::basic_string<basic_tag_t> run_;
	struct hasher{
		std::hash<std::u16string> internal_hasher_;
		std::size_t operator()(const std::basic_string<basic_tag_t> & s) const {
		  return internal_hasher_( (std::u16string::value_type*) s.data());
		}
	};
	std::unordered_map<std::basic_string<basic_tag_t>, basic_tag_t,hasher> lookup_;

	static constexpr basic_tag_t TAG_EOF    = 0;

	static constexpr basic_tag_t TAG_INT32  = 1;
	static constexpr basic_tag_t TAG_DOUBLE = 2;
	static constexpr basic_tag_t TAG_ASCII  = 4;
	static constexpr basic_tag_t TAG_INT64  = 8;
	static constexpr basic_tag_t TAG_INT16  = 16;

	static constexpr basic_tag_t TAG_DOWN = 32;
	static constexpr basic_tag_t TAG_UP   = 64;
	static constexpr basic_tag_t TAG_L_DOWN   = 128;
	static constexpr basic_tag_t TAG_L_UP     = 256;


	std::uint16_t ngram_counter_ = 0;

	mutable std::vector<basic_tag_t> ngrams;
	mutable std::vector<std::size_t> codes2ngrams;

	size_t mutable cur_ofs_ = 0;
	size_t mutable cur_type_ofs_ = 0;
	bool mutable inside_type = false;
	size_t mutable type_idx = 0;

	ast_t(char* data, char* type_info, std::size_t data_size, std::size_t type_info_size): data_{data}, type_info_{type_info}, data_size_{data_size},type_size_{type_info_size}
	{
	
	}

	static bool is_composite(basic_tag_t code) { return code & 0x8000; }

	std::size_t max_ngram_len()  {
		size_t r = 0;
		size_t c = 0;
		if (!ngrams.size())
			prepare_reading();
		for (auto const & e : ngrams){
			if (e == TAG_EOF)
			{
				if (c > r) r = c;
				c = 0;
			}
			else ++c;
		}
		if (c > 0 && c > r) return c;
		return r;
	}

	std::size_t ngram_count() {
		size_t r = 0;
		size_t c = 0;

		if (!ngrams.size())
			prepare_reading();
		for (auto const& e : ngrams) {
			if (e == TAG_EOF)
			{
				if (c > 0) ++r;
				c = 0;
			}
			else ++c;
		}
		if (c > 0 ) return c+1;
		return r;
	}

	bool get(std::uint16_t & elem_type, char** data, size_t& len) const {
		if (current_data_beg_ <= cur_ofs_) return false;
		std::uint16_t code = elem_type = TAG_EOF;
		for (; cur_type_ofs_ < current_type_len_; cur_type_ofs_+= sizeof(std::uint16_t)) {
			code = *(std::uint16_t*)(type_info_ + cur_type_ofs_);
			if (code & 0x8000) {
				auto ngram_code = code & 0x7FFF;
				if (inside_type) {
					++type_idx;
					auto t = ngrams[codes2ngrams[ngram_code] + type_idx];
					if (t == TAG_EOF) {
						//cur_type_ofs_  += sizeof(std::uint16_t);
						inside_type = false;
						continue;
					}
					else elem_type = ngrams[codes2ngrams[ngram_code] + type_idx];
				}
				else {
					inside_type = true;
					type_idx = 0;
					elem_type = ngrams[codes2ngrams[ngram_code]];
				}
			}
			else {
				elem_type = code;
				cur_type_ofs_ += sizeof(std::uint16_t);
			}
			break;
		}
		if (elem_type == TAG_EOF) return false;
		*data = data_ + cur_ofs_;
		if (elem_type == TAG_INT32) { len = sizeof(std::int32_t); cur_ofs_+= sizeof(std::int32_t);}
		else if (elem_type == TAG_INT16) { len = sizeof(std::int16_t); cur_ofs_ += sizeof(std::int16_t);}
		else if (elem_type == TAG_INT64) { len = sizeof(std::int64_t); cur_ofs_ += sizeof(std::int64_t);}
		else if (elem_type == TAG_DOUBLE) { len = sizeof(double); cur_ofs_ += sizeof(double);}
		else if (elem_type == TAG_ASCII) { len = *(std::size_t*)*data + 1; data += sizeof(std::size_t); cur_ofs_ += sizeof(std::size_t) + len;}
		return true;
	}

	void reset() { cur_ofs_ = 0; cur_type_ofs_ = 0; current_type_len_ = type_info_cur_; }

	void prepare_reading()  {
		codes2ngrams.resize(ngram_counter_ + 1);
		for (auto const& e : lookup_) {
			codes2ngrams[e.second] = ngrams.size();
			for (auto ee : e.first) ngrams.push_back(ee);
			ngrams.push_back(TAG_EOF);
		}
		reset();
	}

	void write_eof() {
		if (run_.length() == 0) return;
		if (run_.length() == 1) *(((std::uint16_t*)type_info_) + type_info_cur_++) = run_[0];
		else *(((std::uint16_t*)type_info_) + type_info_cur_++) = 0x8000 | lookup_[run_] ;
	}

	void write_type_tag(std::uint16_t tag) {
		run_.push_back(tag); if (run_.length() == 1) return;
		auto it = lookup_.find(run_);
		if (it == lookup_.end()) {
			lookup_[run_] = ++ngram_counter_;
			if (run_.length() == 2) {
				*(((std::uint16_t*)type_info_) + type_info_cur_++) = run_[0];
				run_ = run_.substr(1);
			} else {
				auto code = lookup_[run_.substr(0,run_.length()-1)];
				*( ((std::uint16_t*)type_info_) + type_info_cur_++) = 0x8000|code;
				run_ = run_.substr(run_.length()-1);
			}
		}
	}

	void write_int_data(std::int32_t value, std::string name = std::string{}) {
		*(std::int32_t*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(std::int32_t);
	}

	void write_int(std::int32_t value, std::string name = std::string{}) {
		write_type_tag(TAG_INT32);
		write_int_data(value,name);
	}

	void write_int16_data(std::int16_t value, std::string name = std::string{}) {
		*(std::int16_t*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(std::int16_t);
	}

	void write_int16(std::int16_t value, std::string name = std::string{}) {
		write_type_tag(TAG_INT16);
		write_int16_data(value, name);
	}

	void write_int64_data(std::int64_t value, std::string name = std::string{}) {
		*(std::int64_t*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(std::int64_t);
	}

	void write_int64(std::int64_t value, std::string name = std::string{}) {
		write_type_tag(TAG_INT64);
		write_int64_data(value,name);
	}

	void write_double_data(double value, std::string name = std::string{}) {
		*(double*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(double);
	}

	void write_double(double value, std::string name = std::string{}) {
		write_type_tag(TAG_DOUBLE);
		write_double_data(value,name);
	}

	void write_ascii_data(std::string const& value, std::string name = std::string{}) {
		*(std::size_t*)(data_ + current_data_beg_) = value.length();
		current_data_beg_ += sizeof(std::size_t);
		if (value.length() == 0) return;
		memcpy(data_ + current_data_beg_, value.c_str(), value.length());
		current_data_beg_ += value.length();
		*(char*)(data_ + current_data_beg_) = 0;
		++current_data_beg_;
	}

	void write_ascii(std::string const & value, std::string name = std::string{}) {
		write_type_tag(TAG_ASCII);
		write_ascii_data(value,name);
	}

	void down() {
		write_type_tag(TAG_DOWN);
	}

	void up() {
		write_type_tag(TAG_UP);
	}

	void ldown() {
		write_type_tag(TAG_L_DOWN);
	}

	void lup() {
		write_type_tag(TAG_L_UP);
	}

};

std::ostream& operator << (std::ostream& os, ast_t& ast) {
	auto handle_simple_types = [&](std::uint16_t type,char* data, std::size_t len) {
		if (type == ast_t::TAG_INT32) os << *(std::int32_t*) data;
		else if (type == ast_t::TAG_DOUBLE) os << *(double*)data;
		else if (type == ast_t::TAG_ASCII) os << data + sizeof(std::size_t);
	};
	std::uint16_t type; std::size_t len; char* data;
	ast.reset();
	for (; ast.get(type, &data, len);) {
		if (!ast_t::is_composite(type))
			handle_simple_types(type,data,len);
		os << "\n";
	}
	return os;
}

void print_debug_info(std::ostream& os, ast_t const& ast) {
	if (!ast.data_) return;
	os << "(";
	for (auto i = 0; i != ast.type_info_cur_; ++i) {
		os << *((std::uint16_t*)ast.type_info_ + i) << " ";
	}
	os << ")(";
	for (auto i = 0; i != ast.current_data_beg_; ++i) {
		os << (unsigned int)((unsigned char*)ast.data_)[i];
		if (i + 1 != ast.current_data_beg_) os << ",";
	}
	os << ")";
	os << "ngrams:\n";
	for (auto const& e : ast.ngrams) os << e << ",";
}


#include <chrono>
std::tuple<std::chrono::microseconds, std::chrono::microseconds,std::size_t,std::size_t,std::size_t,std::size_t,std::size_t> speed_test(unsigned int loops = 100) {
	size_t data_size = 100*1024 * 1024;
	size_t typeinfo_size = data_size;
	char* data_buffer = new char[data_size]{ 0 };
	char* type_buffer = new char[typeinfo_size]{ 0 };
	std::size_t bytes_written = 0;
	size_t max_ngram_len = 0;
	size_t ngram_count = 0;
	size_t payload = 0;
	size_t type_info_size = 0;

	{
		ast_t ast{ data_buffer, type_buffer, data_size, typeinfo_size };
		for (auto j = 0; j < 2*1024 * 1024; ++j) {
			ast.write_int(j);
			ast.write_int(j);
			ast.write_int(j);
			ast.write_double(++j + 0.5);
			ast.write_ascii("Hello!");
			ast.write_int(++j);
			ast.write_ascii("Hello!");
		}
		max_ngram_len = ast.max_ngram_len();
		ngram_count = ast.ngram_count();
		bytes_written = (payload = ast.current_data_beg_)*loops;
		type_info_size = ast.type_info_cur_;
	}

	auto t1_1 = std::chrono::high_resolution_clock::now();

	for (unsigned int loop_ctr = 0; loop_ctr < loops; ++loop_ctr) {
		ast_t ast{ data_buffer, type_buffer, data_size, typeinfo_size };
		for (auto j = 0; j < 2*1024*1024; ++j) {
			ast.write_int(j);
			ast.write_int(j);
			ast.write_int(j);
			ast.write_double(++j + 0.5);
			ast.write_ascii("Hello!");
			ast.write_int(++j);
			ast.write_ascii("Hello!");
		}
		ast.write_eof();
	}

	auto t2_1 = std::chrono::high_resolution_clock::now();

	auto runtime_with_compression = std::chrono::duration_cast<std::chrono::microseconds>(t2_1 - t1_1);

	auto t1_2 = std::chrono::high_resolution_clock::now();

	for (unsigned int loop_ctr = 0; loop_ctr < loops; ++loop_ctr) {
		ast_t ast{ data_buffer, type_buffer, data_size, typeinfo_size };
		for (auto j = 0; j < 2 * 1024 * 1024; ++j) {
			ast.write_int_data(j);
			ast.write_int_data(j);
			ast.write_int_data(j);
			ast.write_double_data(++j + 0.5);
			ast.write_ascii_data("Hello!");
			ast.write_int_data(++j);
			ast.write_ascii_data("Hello!");
		}
		ast.write_eof();
	}

	auto t2_2 = std::chrono::high_resolution_clock::now();
	auto runtime_no_compression = std::chrono::duration_cast<std::chrono::microseconds>(t2_2 - t1_2);

	return { runtime_no_compression , runtime_with_compression, bytes_written, max_ngram_len,ngram_count,payload,type_info_size };
}

int main()
{
	{
		unsigned int loops = 10;
		auto result = speed_test(loops);
		auto mib_written = (double)std::get<2>(result) / (double)(1024 * 1024);
		auto avg_time_compressed = ((double)std::get<1>(result).count() / 1000000.0) / (double)loops;
		auto avg_time_uncompressed = ((double)std::get<0>(result).count() / 1000000.0) / (double)loops;
		auto max_ngram_len = std::get<3>(result);
		auto ngram_count = std::get<4>(result);
		auto payload = std::get<5>(result);
		auto typeinfo_size = std::get<6>(result);

		std::cout << "Test 1:\n";
		std::cout << "Length (in primitive 1-grams) of longest n-gram:" << max_ngram_len << "\n";
		std::cout << "Number of composite n-grams:" << ngram_count<< "\n";
		std::cout << "Payload (size of raw data):" << payload << " bytes\n";
		std::cout << "Type Information :" << typeinfo_size << " bytes\n";
		std::cout << "Bytes written (total):" << mib_written << " MiB\n";
		std::cout << "Elapsed Time per write (with type compression, on average):" << avg_time_compressed << " seconds" << " that is "<< mib_written/ (loops*avg_time_compressed)  << " MiB/sec\n";
		std::cout << "Elapsed Time per write (without type compression, on average):" << avg_time_uncompressed << " seconds" << " that is " << mib_written / (loops*avg_time_uncompressed) << " MiB/sec\n";
	}
}
