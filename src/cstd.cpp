#include "indexed_heap.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
using namespace std;
using Clock = chrono::steady_clock;

static constexpr U subset_budget = 65536;
static constexpr U bitmap_budget_bytes = 8ULL << 30;

struct Block {uint32_t word; U bits;};
struct Row {vector<U> full; vector<Block> blocks;};
struct Graph {
    vector<vector<int>> edges,inc,adj;
    vector<Row> rows;
    vector<U> active;
    int n=0,W=0;
    void read(const string& path){
        ifstream f(path);if(!f)throw runtime_error("cannot open input");
        string line;int V=0;
        while(getline(f,line)){
            if(line.empty())continue;
            vector<int> edge;long long x=0;bool have=false;
            for(char c:line){
                if(c>='0'&&c<='9'){x=x*10+c-'0';have=true;if(x>INT32_MAX-1)throw runtime_error("vertex ID overflow");}
                else if(c==','||c==' '||c=='\t'||c=='\r'){
                    if(have){edge.push_back(int(x));x=0;have=false;}}
                else throw runtime_error("invalid input field");
            }
            if(have)edge.push_back(int(x));
            sort(edge.begin(),edge.end());edge.erase(unique(edge.begin(),edge.end()),edge.end());
            if(edge.empty())throw runtime_error("empty hyperedge");
            V=max(V,edge.back()+1);edges.push_back(move(edge));
        }
        if(edges.size()>INT32_MAX-1)throw runtime_error("too many hyperedges");
        n=int(edges.size());W=(n+63)/64;inc.resize(V);
        for(int e=0;e<n;++e)for(int v:edges[e])inc[v].push_back(e);
        active.assign(W,~U(0));if(n%64)active.back()=(U(1)<<(n%64))-1;
    }
    bool alive(int v)const{return (active[v/64]>>(v%64))&1;}
    void adjacency(){
        adj.resize(n);vector<int>stamp(n,-1);
        for(int u=0;u<n;++u){
            stamp[u]=u;auto&a=adj[u];
            for(int x:edges[u])for(int v:inc[x])if(stamp[v]!=u){stamp[v]=u;a.push_back(v);}
            sort(a.begin(),a.end());
        }
    }
    void make_rows(){
        rows.resize(n);U used=0;size_t threshold=max(64,W);
        vector<int>order(n);iota(order.begin(),order.end(),0);
        stable_sort(order.begin(),order.end(),[&](int a,int b){return adj[a].size()>adj[b].size();});
        for(int u:order){
            if(adj[u].size()<threshold)continue;
            size_t nblock=0;int last=-1;
            for(int x:adj[u])if(x/64!=last){last=x/64;++nblock;}
            U dense_bytes=U(W)*8,block_bytes=U(nblock)*sizeof(Block);
            // Full rows win for dense ID coverage. A compressed word directory
            // serves scattered dense regions; low-density rows remain sorted lists.
            if(dense_bytes<=block_bytes*2 && dense_bytes<=bitmap_budget_bytes-used){
                auto&r=rows[u].full;r.assign(W,0);
                for(int x:adj[u])r[x/64]|=U(1)<<(x%64);
                used+=dense_bytes;
            }else if(block_bytes<U(adj[u].size())*sizeof(int) && block_bytes<=bitmap_budget_bytes-used){
                auto&r=rows[u].blocks;r.reserve(nblock);
                for(int x:adj[u]){uint32_t w=x/64;if(r.empty()||r.back().word!=w)r.push_back({w,0});r.back().bits|=U(1)<<(x%64);}
                used+=block_bytes;
            }
        }
    }
    U shared_mask(int u,int v)const{
        U mask=0;size_t a=0,b=0;
        while(a<edges[u].size()&&b<edges[v].size()){
            if(edges[u][a]==edges[v][b]){if(a<64)mask|=U(1)<<a;++a;++b;}
            else if(edges[u][a]<edges[v][b])++a;else ++b;
        }return mask;
    }
    bool dense(int u,int v,int w)const{
        for(int x:edges[u])if(binary_search(edges[v].begin(),edges[v].end(),x)&&binary_search(edges[w].begin(),edges[w].end(),x))return true;
        return false;
    }
    U common(int u,int v) const{
        U count=0;
        const auto&a=rows[u];const auto&b=rows[v];
        auto pop=[&](U z,uint32_t w){return U(__builtin_popcountll(z&active[w]));};
        if(!a.full.empty()&&!b.full.empty()){
            for(int w=0;w<W;++w)count+=pop(a.full[w]&b.full[w],w);
        }else if((!a.full.empty()&&!b.blocks.empty())||(!b.full.empty()&&!a.blocks.empty())){
            const auto&full=a.full.empty()?b.full:a.full;const auto&blocks=a.full.empty()?a.blocks:b.blocks;
            for(auto z:blocks)count+=pop(z.bits&full[z.word],z.word);
        }else if(!a.blocks.empty()&&!b.blocks.empty()){
            size_t i=0,j=0;
            while(i<a.blocks.size()&&j<b.blocks.size()){
                if(a.blocks[i].word==b.blocks[j].word){count+=pop(a.blocks[i].bits&b.blocks[j].bits,a.blocks[i].word);++i;++j;}
                else if(a.blocks[i].word<b.blocks[j].word)++i;else ++j;
            }
        }else if(!a.full.empty()||!b.full.empty()){
            const auto&full=a.full.empty()?b.full:a.full;const auto&list=a.full.empty()?adj[u]:adj[v];
            for(int x:list)if(alive(x)&&((full[x/64]>>(x%64))&1))++count;
        }else if(!a.blocks.empty()||!b.blocks.empty()){
            const auto&blocks=a.blocks.empty()?b.blocks:a.blocks;const auto&list=a.blocks.empty()?adj[u]:adj[v];
            size_t j=0;for(int x:list){uint32_t w=x/64;
                while(j<blocks.size()&&blocks[j].word<w)++j;
                if(j==blocks.size())break;
                if(blocks[j].word==w&&alive(x)&&((blocks[j].bits>>(x%64))&1))++count;}
        }else{
            const vector<int>*small=&adj[u],*large=&adj[v];if(small->size()>large->size())swap(small,large);
            if(small->size()*8<large->size()){
                auto it=large->begin();for(int x:*small){it=lower_bound(it,large->end(),x);if(it==large->end())break;
                    if(*it==x&&alive(x))++count;}
            }else{size_t i=0,j=0;while(i<small->size()&&j<large->size()){
                int x=(*small)[i],y=(*large)[j];
                if(x==y){if(alive(x))++count;++i;++j;}else if(x<y)++i;else ++j;}}
        }
        return count;
    }
};

struct FrequentCounter {
    const Graph&g;
    vector<__int128>sum;
    vector<U>visits;
    vector<uint8_t>fallback;
    struct Item {int vertex;vector<int>posting;};
    explicit FrequentCounter(const Graph&graph):g(graph),sum(g.n,0),visits(g.n,0),fallback(g.n,0){}
    bool term(const vector<int>&posting,int depth){
        __int128 d=posting.size()-1,contribution=d*(d-1)/2;if(depth%2==0)contribution=-contribution;
        bool any=false;
        for(int e:posting)if(!fallback[e]){
            if(++visits[e]>subset_budget){fallback[e]=1;sum[e]=0;}
            else{sum[e]+=contribution;any=true;}
        }return any;
    }
    void dfs(const vector<Item>&items,int depth){
        for(size_t i=0;i<items.size();++i){
            const auto&p=items[i].posting;if(!term(p,depth))continue;
            vector<Item>next;
            for(size_t j=i+1;j<items.size();++j){
                vector<int>common;const auto&q=items[j].posting;
                size_t a=0,b=0;while(a<p.size()&&b<q.size()){
                    if(p[a]==q[b]){common.push_back(p[a]);++a;++b;}else if(p[a]<q[b])++a;else ++b;
                }
                if(common.size()>=3)next.push_back({items[j].vertex,move(common)});
            }
            if(!next.empty())dfs(next,depth+1);
        }
    }
    vector<U>dense_support(){
        // The root extension lists are generated only from co-occurring vertices,
        // so a million-vertex input never incurs a quadratic all-pairs root scan.
        for(int v=0;v<int(g.inc.size());++v){
            const auto&p=g.inc[v];if(p.size()<3||!term(p,1))continue;
            unordered_map<int,vector<int>>extensions;
            for(int e:p)for(int w:g.edges[e])if(w>v&&g.inc[w].size()>=3)extensions[w].push_back(e);
            vector<Item>items;
            for(auto&entry:extensions)if(entry.second.size()>=3)items.push_back({entry.first,move(entry.second)});
            sort(items.begin(),items.end(),[](const Item&a,const Item&b){return a.vertex<b.vertex;});
            if(!items.empty())dfs(items,2);
        }
        vector<U>s(g.n);
        for(int e=0;e<g.n;++e)if(!fallback[e]){
            if(sum[e]<0||sum[e]>numeric_limits<U>::max())throw runtime_error("dense support overflow/invariant");
            s[e]=U(sum[e]);
        }
        return s;
    }
    void sparse_support(vector<U>&s){
        vector<vector<int>>conn(g.inc.size());vector<int>stamp(g.inc.size(),-1);
        vector<U>mask(g.n),seen(g.n,0);U epoch=0;
        for(int u=0;u<g.n;++u){
            for(int v:g.adj[u])if(v>u){
                mask[v]=g.shared_mask(u,v);
                for(int x:g.edges[v])if(!binary_search(g.edges[u].begin(),g.edges[u].end(),x)){
                    if(stamp[x]!=u){stamp[x]=u;conn[x].clear();}conn[x].push_back(v);}
            }
            for(int v:g.adj[u])if(v>u){
                ++epoch;
                for(int x:g.edges[v])if(!binary_search(g.edges[u].begin(),g.edges[u].end(),x)){
                    auto&list=conn[x];
                    for(auto it=upper_bound(list.begin(),list.end(),v);it!=list.end();++it){
                        int w=*it;
                        if(seen[w]==epoch||(g.edges[u].size()<=64?(mask[v]&mask[w])!=0:g.dense(u,v,w)))continue;
                        seen[w]=epoch;
                        if(!fallback[u])++s[u];if(!fallback[v])++s[v];if(!fallback[w])++s[w];
                    }
                }
            }
        }
    }
    vector<U> count() {
        vector<U> supports = dense_support();
        U dense_sum = accumulate(supports.begin(), supports.end(), U(0));

        // Recover the complete support for each edge that exceeded the subset budget.
        for (int u = 0; u < g.n; ++u) {
            if (!fallback[u]) continue;
            U all = 0;
            U dense = 0;
            for (int v : g.adj[u]) {
                auto a = upper_bound(g.adj[v].begin(), g.adj[v].end(), v);
                auto b = upper_bound(g.adj[u].begin(), g.adj[u].end(), v);
                while (a != g.adj[v].end() && b != g.adj[u].end()) {
                    if (*a == *b) {
                        ++all;
                        if (g.dense(u, v, *a)) ++dense;
                        ++a;
                        ++b;
                    } else if (*a < *b) {
                        ++a;
                    } else {
                        ++b;
                    }
                }
            }
            supports[u] = all;
            dense_sum += dense;
        }

        if (dense_sum % 3) throw runtime_error("dense support divisibility invariant");
        sparse_support(supports);
        return supports;
    }
};

int main(int argc, char** argv) {
    if (argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h")) {
        cout << "Usage: cstd INPUT\n"
             << "INPUT contains one hyperedge per line, with comma- or space-separated vertex IDs.\n";
        return 0;
    }
    if (argc != 2) {
        cerr << "Usage: cstd INPUT\n";
        return 2;
    }

    try {
        const auto start = Clock::now();
        Graph graph;
        graph.read(argv[1]);
        graph.adjacency();
        graph.make_rows();

        vector<U> supports = FrequentCounter(graph).count();
        IndexedHeap heap(supports);
        vector<U> answer(graph.n, 2);
        while (!heap.h.empty()) {
            const int edge = heap.pop();
            const U level = supports[edge];
            answer[edge] = level + 2;
            graph.active[edge / 64] &= ~(U(1) << (edge % 64));
            for (int neighbor : graph.adj[edge]) {
                if (!graph.alive(neighbor) || supports[neighbor] <= level) continue;
                const U loss = graph.common(edge, neighbor);
                if (loss) {
                    const U support = supports[neighbor];
                    heap.decrease(neighbor, support - level > loss ? support - loss : level);
                }
            }
        }

        const double elapsed = chrono::duration<double>(Clock::now() - start).count();
        for (int i = 0; i < graph.n; ++i) {
            cout << "Hyperedge ID: " << i << ", Trussness: " << answer[i] << '\n';
        }
        cerr << "hyperedges=" << graph.n;
        if (!answer.empty()) {
            const auto [minimum, maximum] = minmax_element(answer.begin(), answer.end());
            cerr << " min_trussness=" << *minimum << " max_trussness=" << *maximum;
        }
        cerr << setprecision(9) << " elapsed_seconds=" << elapsed << '\n';
        return 0;
    } catch (const exception& error) {
        cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
