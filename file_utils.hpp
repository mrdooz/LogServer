#pragma once

bool load_file(const char *filename, void **buf, size_t *size);
bool file_exists(const char *filename);
std::string replace_extension(const char *org, const char *new_ext);
std::string strip_extension(const char *str);
void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext);
