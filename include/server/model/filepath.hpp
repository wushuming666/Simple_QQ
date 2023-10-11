#ifndef FILEPATH_H
#define FILEPATH_H

#include <string>

using namespace std;

//匹配User表的ORM类
class Filepath
{
public:
    Filepath(int id=-1, string path="")
    {
        this->id = id;
		this->path = path;    
	}

    void setId(int id) {this->id = id;}
	void setPath(string path) {this->path = path;}

	int getId() {return this->id;}
	string getPath() {return this->path;}
protected:
    int id;
    string path;
};

#endif
