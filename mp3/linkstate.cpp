#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <climits>
using namespace std;

map<int, map<int, int>> weights;
map<int, map<int, int>> shortest_path;
map<int, map<int, list<int>>> seq;

set<int> v_set;
set<int> v;

list<string> msgs;
list<string> changes;

set<int> v_delete;

void linkstate_procedure(){
    shortest_path.clear();
    seq.clear();
    //Dijkstra's algorithm
    for (auto idx = v_set.begin(); idx != v_set.end(); idx++){
        int i = *idx;
        if (v.count(i) == 1){
            map<int, int> dist;
            priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int,int>>> path;

            for (int n : v_set)
                dist[n] = INT_MAX;

            path.push(make_pair(0, i));
            dist[i] = 0;

            while (!path.empty()) {
                int u = path.top().second;
                path.pop();

                map<int, int>::iterator k;
                for (k = weights[u].begin(); k != weights[u].end(); k++){
                    int v = k->first;
                    int weight = k->second;

                    if (dist[v] > dist[u] + weight) {
                        dist[v] = dist[u] + weight;
                        path.push(make_pair(dist[v], v));

                        shortest_path[i][v] = dist[v];
                        seq[i][v] = seq[i][u];
                        seq[i][v].push_back(v);
                    }
                }
            }

            ofstream fpOut;
            fpOut.open("output.txt", ios::app);
            
            for (auto idx = seq[i].begin(); idx != seq[i].end(); idx++){
                int dest = idx->first;
                int next_hop = idx->second.front();
                int weight = shortest_path[i][dest];
                fpOut << dest << " " << next_hop << " " << weight << endl;
            }
            fpOut.close();
        }
        else{
            ofstream fpOut;
            fpOut.open("output.txt", ios::app);
            fpOut << i << " " << i << " " << 0 << endl;
            fpOut.close();
        }
    }
    ofstream fpOut;
    fpOut.open("output.txt", ios::app);
    for (string& s: msgs){
        int src, dest;
        stringstream temp(s);
        temp >> src; 
        temp >> dest;
        string msg(s, s.find(to_string(dest)) + 2);

        if (shortest_path[src][dest] == INT_MAX || seq[src][dest].size() == 0)
            fpOut << "from " << src << " to " << dest << " cost infinite hops unreachable message " << msg << endl;
        else{
            list<int> sequence = seq[src][dest];
            fpOut << "from " << src << " to " << dest << " cost " << shortest_path[src][dest] << " hops " << src << " "; 
            sequence.pop_back();
            for (auto idx = sequence.begin(); idx != sequence.end(); idx++){
                int i = *idx;
                fpOut << i << " ";
            }
            fpOut << "message " << msg << endl;
        }
    }
    fpOut.close();
    return;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fclose(fpOut);

    ifstream input_file;
    string line;

    //Topology 
    input_file.open(argv[1]);
    while (getline(input_file, line)){
        int node1, node2, weight;
        stringstream topology(line);
        topology >> node1;
        topology >> node2;
        topology >> weight;
        v.insert(node1);
        v.insert(node2);
        weights[node1][node2] = weight;
        weights[node2][node1] = weight;
    }

    //Messages
    input_file.close();
    v_set = v;

    input_file.open(argv[2]);
    while (getline(input_file, line)){
        msgs.push_back(line);
    }
    input_file.close();

    //Changes
    input_file.open(argv[3]);
    while (getline(input_file, line)){
        changes.push_back(line);
    }        
    input_file.close();

    linkstate_procedure();
    for (auto idx = changes.begin(); idx != changes.end(); idx++) {
        string &s = *idx;
        stringstream ss(s);
        int v1, v2, weight;
        ss >> v1 >> v2 >> weight;

        if (weight == -999){
            weights[v1].erase(v2);
            weights[v2].erase(v1);

            set<int> v_new;
            for (auto i = weights.begin(); i != weights.end(); i++) {
                for (auto j = weights.begin(); j != weights.end(); j++) {
                    v_new.insert(i->first);
                    v_new.insert(j->first);
                }
            }
            for (auto it = v.begin(); it != v.end(); it++){
                int i = *it;
                if (v_new.count(i) == 0){
                    v_delete.insert(i);
                }
            }
            v = v_new;
        }
        else{
            v.insert(v1);
            v.insert(v2);
            weights[v1][v2] = weight;
            weights[v2][v1] = weight;       
            if (v_delete.count(v1) == 1){ v_delete.erase(v1); }
            if (v_delete.count(v2) == 1){ v_delete.erase(v2); }    
        }
        linkstate_procedure();
    }
    return 0;
}