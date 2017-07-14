/*
 * tree.hpp
 *
 *  Created on: Jan 20, 2015
 *      Author: nachshonc
 */

#ifndef TREE_HPP_
#define TREE_HPP_
#include <string>
class tree{
public:
	virtual bool search(unsigned key)=0;
	virtual bool insert(unsigned key){search(key); return false; }
	virtual bool remove(unsigned key){search(key); return false; }
	virtual ~tree(){}
	virtual std::string name(){return "none"; }
};


#endif /* TREE_HPP_ */
