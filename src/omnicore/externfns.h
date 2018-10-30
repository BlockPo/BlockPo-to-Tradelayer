#ifndef EXTERNFNS_H
#define EXTERNFNS_H

#include "tradelayer_matrices.h"
#include <vector>
#include <unordered_set>

void printing_matrix(MatrixTLS &gdata);
void printing_vector(VectorTLS &vdata);
bool finding(std::string &s, VectorTLS &v);
void sub_row(VectorTLS &jrow_databe, MatrixTLS &databe, int i);
bool is_number(const std::string& s);
bool finding_string(std::string sub_word, std::string word_target);
bool find_string_strv(std::string s, std::vector<std::string> v);
bool find_string_set(std::string s, std::unordered_set<std::string> addrs_set);
bool findTrueValue(bool a, bool b, bool c);
bool findTrueValue(bool a, bool b, bool c, bool d);

#endif
