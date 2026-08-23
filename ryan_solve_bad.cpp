#include <bits/stdc++.h>
using namespace std;

enum Stage {
  NO_ACTIVE,

  NEED_PPRE,
  NEED_PPROC,
  NEED_PPOST,

  NEED_DPRE,
  NEED_DPROC,
  NEED_DPOST,

  BLOCKED
};

int main() {
  ios::sync_with_stdio(false); // cin sync with printf, prevent for speed
  cin.tie(nullptr); // disable automatic flushes for speed
  
  // Reading the startup config
  int K; // number of remote machines
  double S; // fixed scheduling cost added to every task
  double latency; // network cost to go UP/DOWN
  double bandwidth; // Gbps, how much can go through
  int bytesPerToken; // data transferred per token, in bytes
  int numLayers; // number of layers for P PROC

  // Scoring parameters
  double SLO1; // target average TDR (Time to decode ready)
  double SLO2; // target average TPOT (Time per output token)
  double tpUB; // output-rate target where throughput score reaches 1
  double tpBase; // baseline output rate where throughput score is 0
  double distBase; // baseline waiting time distance where score is 0
                   // having distBase of 0 would just give us 1 score
                   // It is for how much TDR and TPOT are off where you would
                   // get 0
  double wTp; // weight given to output-rate score
  double wC; // weight for constraint like the waiting time TDR and TPOT

  cin >> K >> S >> latency >> bandwidth >> bytesPerToken >> numLayers;
  cin >> SLO1 >> SLO2 >> tpUB >> tpBase >> distBase >> wTp >> wC;

  // read and ignore the complete task-time table:
  int N;
  cin >> N;
  double tableValue;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < 7; ++j) {
      cin >> tableValue;
    }
  }

  // current scheduling policy is:
  //   Only one request active at a time. Every request assigned to C0. P
  //   Proc always uses the full [0, numLayers) piece. Every D task has group 
  //   size of 1
  deque<int> arrivals; // can remove and add from front and back. Basically a 
  // queue but you also have the ability to index at any place in O(1)
  int active = -1; // stores the request ID currently being processed
  Stage stage = NO_ACTIVE; // what stage of process is current request on
  bool edgeFree = true; // is the local computer free
  bool c0Free = true; // is the remote computer free

  // We will keep reading one complete event frame
  while (true) {
    string firstToken;
    cin >> firstToken;
    if (firstToken == "END") {
      break;
    }
    // firstToken was the timestamp.
    int eventCount;
    cin >> eventCount;

    // Read entire frame before scheduling
    // this is where we update all our states
    for (int event = 0; event < eventCount; ++event) {
      string type;
      cin >> type;

      // of form ARR rid Lin where rid = request id and Lin is the length
      // of the input
      if (type == "ARR") {
        int rid, Lin;
        cin >> rid >> Lin;
        // we ignore Lin because judge already knows and uses it for 
        // tasks and transfer times for prefill stage
        arrivals.push_back(rid);
      }

      // FIN rid
      else if (type == "FIN") {
        int rid;
        cin >> rid;
        if (rid == active) { // we are no longer using this anymore
          active = -1;
          stage = NO_ACTIVE;
        }
      }

      // XDN <UP|DOWN> <remote> <size> <PRE|DEC> <m> <rid...>
      else if (type == "XDN") {
        // transfer completed
        string direction, phase;
        int remote, m;
        long long sizeBytes;
        cin >> direction >> remote >> sizeBytes >> phase >> m;
        for (int i = 0; i < m; ++i) {
          int rid;
          cin >> rid;
          if (rid != active) {
            // this shouldn't happen because we only have one active at a time
            continue;
          }
          // we are going forward in the stages because transfer happened
          if (phase == "PRE") {
            if (direction == "UP") {
              stage = NEED_PPROC;
            } else {
              stage = NEED_PPOST;
            }
          } else {
            // phase is DEC
            if (direction == "UP") {
              stage = NEED_DPROC;
            } else {
              stage = NEED_DPOST;
            }
          }
        }
      }
      // TDN <server> <task_spec> <dur>
      // task_spec could actually be quite long
      // frees a server (server is either edge or C0 or something)
      else if (type == "TDN") {
        string server, phase, step;
        cin >> server >> phase >> step;
        if (server == "E") {
          edgeFree = true;
        } else if (server == "C0") {
          c0Free = true;
        }
        
        // P task is completed
        if (phase == "P") {
          if (step == "PROC") {
            int ls, le, remote, rid;
            double duration;
            cin >> ls >> le >> remote >> rid >> duration;
            // automatically down transfer is queued and we are blocked
          } 
          else {
            int remote, rid;
            double duration;
            cin >> remote >> rid >> duration;
            if (step == "POST" && rid == active) {
              stage = NEED_DPRE;
            }
          }
          // we can't do anything for P PRE b/c XDN UP blocks
        }
        
        // D task is completed:
        else {
          int markerOrRemote, m; // because either it is -1 or for PROC it 
                                 // is the actual remote computer finished on
          cin >> markerOrRemote >> m;
          bool containsActive = false; // does one of the machines finished active
          for (int i = 0; i < m; ++i) {
            int rid;
            cin >> rid;
            if (rid == active) {
              containsActive = true;
            }
          }
            double duration;
            cin >> duration;
            if (step == "POST" && containsActive) {
              // next token iteration may occur but we could still get FIN
              // so that can still override it later
              stage = NEED_DPRE;
            }
            // D PRE/PROC completion queues a transfer
        }
      }
    }
    // if previous request finished we can admit oldest arrival next in queue
    // should only happen after entire frame is read
    if (active == -1 && !arrivals.empty()) {
      active = arrivals.front();
      arrivals.pop_front();
      stage = NEED_PPRE;
    }

    // only one assignment can be produced because we only have one active
    // We need to schedule the next legal task
    vector<string> answer;
    if (active != -1) { // we should have one request doing something
      // or else we don't have a request
      if (stage == NEED_PPRE && edgeFree) {
        answer.push_back("E P PRE 0 " + to_string(active));
        edgeFree = false;
        stage = BLOCKED;
      } else if (stage == NEED_PPROC && c0Free) {
        answer.push_back("C0 P PROC 0 " + to_string(numLayers) + 
                        " 0 " + to_string(active));
        c0Free = false;
        stage = BLOCKED;
      } else if (stage == NEED_PPOST && edgeFree) {
        answer.push_back("E P POST 0 " +  to_string(active));
        edgeFree = false;
        stage = BLOCKED;
      } else if (stage == NEED_DPRE && edgeFree) {
        answer.push_back("E D PRE -1 1 " + to_string(active));
        edgeFree = false;
        stage = BLOCKED;
      } else if (stage == NEED_DPROC && c0Free) {
        answer.push_back("C0 D PROC 0 1 " + to_string(active));
        c0Free = false;
        stage = BLOCKED;
      } else if (stage == NEED_DPOST && edgeFree) {
        answer.push_back("E D POST -1 1 " + to_string(active));
        edgeFree = false;
        stage = BLOCKED;
      }
    } 
    // We will print the one response and then flush
    cout << answer.size() << '\n';
    for (const string &line : answer) {
      cout << line << '\n';
    }
    cout << flush;
  }
  return 0;
}
