#pragma once
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>
using U = uint64_t;
struct IndexedHeap {
    std::vector<int> h,pos;
    std::vector<U>& key;
    explicit IndexedHeap(std::vector<U>& k):h(k.size()),pos(k.size()),key(k) {
        std::iota(h.begin(),h.end(),0); std::iota(pos.begin(),pos.end(),0);
        for(int i=int(h.size())/2-1;i>=0;--i) down(i);
    }
    bool less(int a,int b)const{return key[a]<key[b]||(key[a]==key[b]&&a<b);}
    void swap_at(int a,int b){std::swap(h[a],h[b]);pos[h[a]]=a;pos[h[b]]=b;}
    void down(int p){while(2*p+1<int(h.size())){int q=2*p+1;
        if(q+1<int(h.size())&&less(h[q+1],h[q]))++q;
        if(!less(h[q],h[p]))break;swap_at(q,p);p=q;}}
    void decrease(int v,U k){key[v]=k;int p=pos[v];
        while(p>0&&less(h[p],h[(p-1)/2])){int q=(p-1)/2;swap_at(p,q);p=q;}}
    int pop(){int v=h[0];swap_at(0,int(h.size())-1);h.pop_back();pos[v]=-1;if(!h.empty())down(0);return v;}
};
