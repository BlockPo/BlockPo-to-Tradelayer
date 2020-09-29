#ifndef EXTERNFNS_H
#define EXTERNFNS_H

#include <tradelayer/log.h>
#include <tradelayer/tradelayer_matrices.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/lexical_cast.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

typedef boost::multiprecision::cpp_dec_float_100 dec_float;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

void printing_matrix(MatrixTLS &gdata);
void printing_vector(VectorTLS &vdata);
bool finding(std::string &s, VectorTLS &v);
void sub_row(VectorTLS &jrow_databe, MatrixTLS &databe, int i);
bool is_number(const std::string& s);
bool finding_string(std::string sub_word, std::string word_target);
bool find_string_strv(std::string s, std::vector<std::string> v);
bool find_string_set(std::string s, std::unordered_set<std::string> addrs_set);
bool findTrueValue(bool a, bool b);
bool findTrueValue(bool a, bool b, bool c);
bool findTrueValue(bool a, bool b, bool c, bool d);
bool findConjTrueValue(bool a, bool b, bool c);
bool findConjTrueValue(bool a, bool b);
std::string DecFloatToString(dec_float& value);
bool find_uint64_t(uint64_t m, std::vector<uint64_t> v);
void threading(uint32_t keyid, int64_t valueid, std::string name_vec);
void vwap_num_den(std::vector<int64_t> vn, std::vector<int64_t> vd, int64_t &n, int64_t &d);

namespace mastercore
{
    int64_t DoubleToInt64(double d);
    int64_t RationalToInt64(rational_t r);
}

template<typename T> void print_stdvector(T const &vec)
{
    typename T::const_iterator pos;
    typename T::const_iterator end(vec.end());

    PrintToLog("\n\nPrinting std::vector: [\t ");
    for (pos=vec.begin(); pos!=end; ++pos) {
        PrintToLog("%s\t", FormatDivisibleMP(*pos));
    }
    PrintToLog("]\n\n");
}

template<typename T> std::vector<T> min_max(std::vector<T> &vec)
{
    auto result = std::minmax_element(vec.begin(),vec.end());
    vec[0]=*result.first;
    vec[1]=*result.second;
    return vec;
}

#endif
