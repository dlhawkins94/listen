#pragma once

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <audiotk/audiotk.hpp>

using namespace std;

typedef vector<bus> dataset;

// loads WAV file sources from data/ whose filename starts with prefix.
// readdir does not guarantee filename order, so the filenames are sorted
// before the sources are opened.
// These sources have to be manually destroyed.
vector<file_source*> load_file_sources(string prefix);

void shuffle_dataset(dataset &d);

struct file_source_info {
  int id, sigtype;
  string desc;
  file_source_info(file_source *src);
};
