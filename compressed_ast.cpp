// compressed_ast.cpp
//
// MIT License
//
// Copyright (c) 2020 Tomas Prerovsky
//
//
//
//


#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>

struct ast_t{
	char* data_ = nullptr;
	char* type_info_ = nullptr;

	std::size_t data_size_ = 0;
	std::size_t type_size_ = 0;

	std::size_t current_data_beg_ = 0;
	std::size_t current_type_beg_ = 0;
	std::size_t current_type_len_ = 0;

	std::size_t type_info_cur_ = 0;

	std::basic_string<std::uint16_t> run_;
	std::unordered_map<std::basic_string<std::uint16_t>, std::uint16_t > lookup_;

	static constexpr std::uint16_t TAG_EOF    = 0;

	static constexpr std::uint16_t TAG_INT32  = 1;
	static constexpr std::uint16_t TAG_DOUBLE = 2;
	static constexpr std::uint16_t TAG_ASCII  = 4;
	static constexpr std::uint16_t TAG_INT64  = 8;
	static constexpr std::uint16_t TAG_INT16  = 16;

	static constexpr std::uint16_t TAG_DOWN = 32;
	static constexpr std::uint16_t TAG_UP   = 64;
	static constexpr std::uint16_t TAG_L_DOWN   = 128;
	static constexpr std::uint16_t TAG_L_UP     = 256;


	std::uint16_t ngram_counter_ = 0;

	std::vector<std::uint16_t> ngrams;
	std::vector<std::size_t> codes2ngrams;

	size_t mutable cur_ofs_ = 0;
	size_t mutable cur_type_ofs_ = 0;
	bool mutable inside_type = false;
	size_t mutable type_idx = 0;

	bool get(std::uint16_t & elem_type, char** data, size_t& len) const {
		if (current_data_beg_ <= cur_ofs_) return false;
		std::uint16_t code = elem_type = TAG_EOF;
		for (; cur_type_ofs_ < current_type_len_; ++cur_type_ofs_) {
			code = *(std::uint16_t*)(type_info_ + cur_type_ofs_);
			if (code & 0x8000) {
				auto ngram_code = code & 0x7FFF;
				if (inside_type) {
					++type_idx;
					auto t = ngrams[codes2ngrams[ngram_code] + type_idx];
					if (t == TAG_EOF) {
						cur_type_ofs_  += sizeof(std::uint16_t);
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

	void prepare_reading() {
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
		prepare_reading();
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
	void write_int(std::int32_t value, std::string name = std::string{}) {
		write_type_tag(TAG_INT32);
		*(std::int32_t*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(std::int32_t);
	}

	void write_int16(std::int16_t value, std::string name = std::string{}) {
		write_type_tag(TAG_INT16);
		*(std::int16_t*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(std::int16_t);
	}

	void write_int64(std::int64_t value, std::string name = std::string{}) {
		write_type_tag(TAG_INT64);
		*(std::int64_t*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(std::int64_t);
	}

	void write_double(double value, std::string name = std::string{}) {
		write_type_tag(TAG_DOUBLE);
		*(double*)(data_ + current_data_beg_) = value;
		current_data_beg_ += sizeof(double);
	}

	void write_ascii(std::string const & value, std::string name = std::string{}) {
		write_type_tag(TAG_ASCII);
		*(std::size_t*)(data_ + current_data_beg_) = value.length();
		current_data_beg_ += sizeof(std::size_t);
		if (value.length() == 0) return;
		memcpy(data_ + current_data_beg_, value.c_str(), value.length());
		current_data_beg_ += value.length();
		*(char*)(data_ + current_data_beg_) = 0;
		++current_data_beg_;
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
	std::uint16_t type;
	std::size_t len;
	char* data;
	ast.reset();
	for (; ast.get(type, &data, len);) {
		if (type == ast_t::TAG_INT32) os << *(std::int32_t*) data;
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
}

int main()
{
	char* data_buffer = new char[32768]  { 0 };
	char* type_buffer = new char[2048]  { 0 };

	ast_t ast{ data_buffer, type_buffer, 4096, 2048 };
	for (auto j = 0; j < 256; ++j) {
		ast.write_int(j);
		ast.write_double(++j);
		ast.write_ascii("Hello!");
		ast.write_int(++j);
	}
	ast.write_eof();

	//print_debug_info(std::cout, ast);


	std::cout << "Decompressed:\n";
	std::cout << ast << "\n\n\n";


}
