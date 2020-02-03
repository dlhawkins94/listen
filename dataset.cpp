#include "dataset.hpp"

vector<file_source*> load_file_sources(string prefix) {
  DIR *dir = opendir("data/"); 
  if (!dir) {
    cerr << "failed to open data directory" << endl;
    exit(1);
  }

  struct dirent *ent;
  vector<string> filenames;
  while (ent = readdir(dir)) {
    string filename(ent->d_name);
    if (!filename.substr(0, prefix.length()).compare(prefix)) {
      vector<string>::iterator curr = filenames.begin();
      while (curr != filenames.end()
	     && filename.compare(*curr) > 0)
	curr++;
      
      filenames.insert(curr, filename);
    }
  }
  
  closedir(dir);

  vector<file_source*> sources;
  for (auto &filename : filenames)
    sources.push_back(new file_source(string("data/") + filename));

  cout << "Opened " << sources.size()
       << " " << prefix << " file sources." << endl;
  return sources;
}

void shuffle_dataset(dataset &d) {
  random_shuffle(d.begin(), d.end());
}

file_source_info::file_source_info(file_source *src) {
  string path = src->get_path();
  vector<string> tokens;
  size_t i = 0;
  size_t j = path.find_first_of('_', i);
  while (j != string::npos) {
    tokens.push_back(path.substr(i,j-i));
    i = j + 1;
    j = path.find_first_of('_', i);
  }
  tokens.push_back(path.substr(i));

  id = atoi(tokens[1].c_str());
  sigtype = atoi(tokens[2].c_str());
  desc = tokens[3];
}
