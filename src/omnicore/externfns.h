#ifndef EXTERNFNS_H
#define EXTERNFNS_H

#include "tradelayer_matrices.h"
#include <vector>
#include <unordered_set>

void printing_matrix(MatrixTL<std::string> &gdata);
void printing_vector(VectorTL<std::string> &vdata);
bool finding(std::string &s, VectorTL<std::string> &v);
void sub_row(VectorTL<std::string> &jrow_databe, MatrixTL<std::string> &databe, int i);
bool is_number(const std::string& s);
bool finding_string(std::string sub_word, std::string word_target);
bool find_string_strv(std::string s, std::vector<std::string> v);
bool find_string_set(std::string s, std::unordered_set<std::string> addrs_set);
bool findTrueValue(bool a, bool b, bool c);

#endif
