/*
 * tree1.hpp
 *
 *  Created on: Jan 18, 2015
 *      Author: nachshonc
 */

#ifndef TREE1_HPP_
#define TREE1_HPP_
#include "tree.hpp"
#include <string>
#include <iostream>
#include <vector>
using namespace std;
class noLockHelper{
public:
	struct node{
		unsigned key;
		node *left, *right;
		void *obj;
	};
	node *head;
	bool search(unsigned key){
		node *cur=head;
		while(cur!=NULL){
			unsigned curkey = cur->key;
			if(key==curkey) return true;
			else if(key<curkey)
				cur=cur->left;
			else
				cur = cur->right;
		}
		return false;
	}
	node *getNewNode(unsigned key){
		node *n=new node();
		n->left=n->right=NULL;
		n->obj=NULL;
		n->key=key;
		return n;
	}
	bool insert(unsigned key){
		node *cur=head;
		if(cur==NULL){//empty tree.
			head=getNewNode(key);
			return true;
		}
		node **pcur;
		do{
			unsigned curkey = cur->key;
			if(key==curkey) {
				return false; //key already exists.
			}
			if(key<curkey)
				pcur=&cur->left;
			else{
				pcur=&cur->right;
			}
			cur=*pcur;
		}while(cur!=NULL);
		*pcur=getNewNode(key);
		return true;
	}

	bool insertGtLn(unsigned key, int *ln){
		node *cur=head;
		if(cur==NULL){//empty tree.
			head=getNewNode(key);
			return true;
		}
		node **pcur;
		do{
			unsigned curkey = cur->key;
			if(key==curkey) {
				return false; //key already exists.
			}
			if(key<curkey)
				pcur=&cur->left;
			else{
				pcur=&cur->right;
			}
			(*ln)++;
			cur=*pcur;
		}while(cur!=NULL);
		*pcur=getNewNode(key);
		return true;
	}
	void deleteNode(node *cur, node **pcur){
		if(cur->right==NULL && cur->left==NULL){
			*pcur=NULL;
			delete cur;
		}
		else if(cur->right==NULL){
			*pcur=cur->left;
			delete cur;
		}
		else if(cur->left==NULL){
			*pcur=cur->right;
			delete cur;
		}
		else{
			node *rchild=cur->right;
			if(rchild->left==NULL){
				rchild->left=cur->left;
				*pcur=rchild;
				delete cur;
			}
			else{
				node *rcur=rchild->left, *rprev=rchild, *rnext;
				rnext = rcur->left;
				while(rnext!=NULL){
					rprev=rcur;
					rcur=rnext;
					rnext=rnext->left;
				}
				rprev->left=rcur->right;//disconnects cur
				//replace cur with rcur
				rcur->left=cur->left;
				rcur->right=cur->right;
				*pcur = rcur;
				delete cur;
			}
		}
	}
	node *removeMin(node *root, node **proot){
		if(root==NULL) return NULL;
		if(root->left==NULL){
			*proot=root->right;
			return root;
		}
		node *rcur=root->left, *rprev=root, *rnext;
		rnext = rcur->left;
		while(rnext!=NULL){
			rprev=rcur;
			rcur=rnext;
			rnext=rnext->left;
		}
		rprev->left=rcur->right;//disconnects cur
		return rcur;
	}
	bool remove(unsigned key){
		node *cur=head;
		node **pcur;
		if(head==NULL) return false;
		if(head->key==key){
			deleteNode(cur, &head);
			return true;
		}
		do{
			unsigned curkey = cur->key;
			//key==curkey cannot happen..
			if(key<curkey)
				pcur=&cur->left;
			else
				pcur=&cur->right;
			cur=*pcur;
		}while(cur!=NULL && cur->key!=key);
		if(cur==NULL) return false;
		deleteNode(cur, pcur);
		return true;
	}

	node *build(std::vector<unsigned>::iterator begin, std::vector<unsigned>::iterator end){
		if(end-begin<=0) return NULL;
		std::vector<unsigned>::iterator mid = begin + (end-begin)/2; //(begin+end)/2;
		node *n = new node();
		n->key = *mid;
		n->obj=NULL;
		n->left=build(begin, mid);
		n->right=build(mid+1, end);
		return n;
	}
	node *build(int i, long first){
		if(i==0) return NULL;
		node *n = new node();
		n->left = build(i-1, first);
		n->right = build(i-1, first + (1<<(i-0)));
		n->key = first + (1<<(i-0));
		n->obj=NULL;
		return n;
	}
	void destroy(node *root){
		if(root==NULL) return;
		destroy(root->left);
		destroy(root->right);
		delete root;
	}
	noLockHelper(int level){
		head=build(level, 0);
	}
	noLockHelper(std::vector<unsigned>::iterator begin, std::vector<unsigned>::iterator end){
		head=build(begin, end);
	}
	int size(node *root){
		if(root==NULL) return 0;
		return size(root->left)+1+size(root->right);
	}
	int addItems(node *root, std::vector<unsigned> &v){
		if(root==NULL) return 0;
		int l = addItems(root->left, v);
		v.push_back(root->key);
		int r = addItems(root->right, v);
		return l+1+r;
	}
	void print(node *head){
		if(head==NULL) return;
		print(head->left);
		printf("%d,", head->key);
		print(head->right);
	}
};

class noLockTree: public tree{
public:
	noLockHelper helper;
	bool search(unsigned key){
		return helper.search(key);
	}
	bool insert(unsigned key){
		return helper.insert(key);
	}
	bool remove(unsigned key){
		return helper.remove(key);
	}
	noLockTree():helper(16){	}
	noLockTree(int lvl):helper(lvl){}
	virtual std::string name(){return "noLock"; }
};
class searchOnlyTree: public noLockTree{
	virtual bool remove(unsigned key){return search(key);}
	virtual bool insert(unsigned key){return search(key);}
	virtual std::string name(){return "searchOnly"; }
};

#endif /* TREE1_HPP_ */

/*
	node *getNewNode(unsigned key){
		node *n=new node();
		n->left=n->right=NULL;
		n->obj=NULL;
		n->key=key;
		return n;
	}
	virtual bool insert(unsigned key){
		node *cur=head;
		if(cur==NULL){//empty tree.
			head=getNewNode(key);
			return true;
		}
		node **pcur;
		do{
			unsigned curkey = cur->key;
			if(key==curkey) {
				return false; //key already exists.
			}
			if(key<curkey)
				pcur=&cur->left;
			else{
				pcur=&cur->right;
			}
			cur=*pcur;
		}while(cur!=NULL);
		*pcur=getNewNode(key);
		return true;
	}
	void deleteNode(node *cur, node **pcur){
		if(cur->right==NULL && cur->left==NULL){
			*pcur=NULL;
			delete cur;
		}
		else if(cur->right==NULL){
			*pcur=cur->left;
			delete cur;
		}
		else if(cur->left==NULL){
			*pcur=cur->right;
			delete cur;
		}
		else{
			node *rchild=cur->right;
			if(rchild->left==NULL){
				rchild->left=cur->left;
				*pcur=rchild;
				delete cur;
			}
			else{
				node *rcur=rchild->left, *rprev=rchild, *rnext;
				rnext = rcur->left;
				while(rnext!=NULL){
					rprev=rcur;
					rcur=rnext;
					rnext=rnext->left;
				}
				rprev->left=rcur->right;//disconnects cur
				//replace cur with rcur
				rcur->left=cur->left;
				rcur->right=cur->right;
				*pcur = rcur;
				delete cur;
			}
		}
	}
	virtual bool remove(unsigned key){
		node *cur=head, *prev=NULL;
		node **pcur;
		if(head==NULL) return false;
		if(head->key==key){
			head=NULL;
			delete cur;
			return true;
		}
		do{
			unsigned curkey = cur->key;
			//key==curkey cannot happen..
			prev=cur;
			if(key<curkey)
				pcur=&cur->left;
			else
				pcur=&cur->right;
			cur=*pcur;
		}while(cur!=NULL && cur->key!=key);
		if(cur==NULL) return false;
		deleteNode(cur, pcur);
		return true;
	}*/
//the code for insert and delete. Still contains bugs.
