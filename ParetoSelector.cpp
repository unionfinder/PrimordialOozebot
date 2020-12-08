#include "ParetoSelector.h"
#include "OozebotEncoding.h"
#include "ParetoFront.h"
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <future>
#include <thread>

// M: # of objectives/dimensions in the pareto front
// N: Size of generation
// K: # of globally undominated solutions found to date
bool sortFunction(OozebotSortWrapper a, OozebotSortWrapper b) {
    return (a.novelty > b.novelty);
}

// Insertion is O(M * N) plus cost of sort - O(N^2) - plus cost of tracking globally O(K)
void ParetoSelector::insertOozebot(OozebotEncoding encoding) {
    OozebotSortWrapper wrapper = {encoding, {}, {}, 0, 0};

    for (std::vector<OozebotSortWrapper>::iterator iter = this->generation.begin(); iter != this->generation.end(); iter++) {
        if (dominates(encoding, (*iter).encoding)) {
            (*iter).dominated.push_back(encoding.id);
            (*iter).dominationDegree += 1;
            wrapper.dominating.push_back((*iter).encoding.id);
        } else if (dominates((*iter).encoding, encoding)) {
            wrapper.dominated.push_back((*iter).encoding.id);
            wrapper.dominationDegree += 1;
            (*iter).dominating.push_back(encoding.id);
        }
    }
    this->idToIndex[encoding.id] = (int) this->generation.size();
    this->generation.push_back(wrapper);
}

void ParetoSelector::removeAllOozebots() {
    this->generation.clear();
    this->idToIndex.clear();
}

std::pair<OozebotEncoding, AsyncSimHandle> gen(OozebotEncoding mom, OozebotEncoding dad, int i, bool shouldMutate) {
    OozebotEncoding child = OozebotEncoding::mate(mom, dad);
    if (shouldMutate) {
        child = mutate(child);
    }
    AsyncSimHandle handle = OozebotEncoding::evaluate(child, i);
    return {child, handle};
}

// Crowding is maintained by dividing the entire
// search space deterministically in subspaces, where is the
// depth parameter and is the number of decision variables, and
// by updating the subspaces dynamically
int ParetoSelector::selectAndMate() {
    this->sort();

    std::vector<OozebotEncoding> newGeneration = {
        this->generation[0].encoding,
        this->generation[1].encoding,
        this->generation[2].encoding,
        this->generation[3].encoding,
        this->generation[4].encoding
    };

    const int asyncThreads = 35;

    std::future<std::pair<OozebotEncoding, AsyncSimHandle>> threads[asyncThreads];
    for (int i = 0; i < asyncThreads; i++) {
        int k = this->selectionIndex();
        int l = this->selectionIndex();
        while (k == l) {
            l = this->selectionIndex();
        }
        threads[i] = std::async(&gen, this->generation[k].encoding, this->generation[l].encoding, i + 1, ((double) rand() / RAND_MAX) < this->mutationProbability);
    }
    int k = this->selectionIndex();
    int l = this->selectionIndex();
    while (k == l) {
        l = this->selectionIndex();
    }
    std::pair<OozebotEncoding, AsyncSimHandle> pair = gen(this->generation[k].encoding, this->generation[l].encoding, 0, ((double) rand() / RAND_MAX) < this->mutationProbability);
    OozebotEncoding encoding = pair.first;
    AsyncSimHandle handle = pair.second;
    
    int j = 0;
    for (int i = 0; i < this->generationSize - 5; i++) {
        auto res = OozebotEncoding::wait(handle);
        encoding.fitness = res.first;
        encoding.lengthAdj = res.second;
        this->globalParetoFront.evaluateEncoding(encoding);
        newGeneration.push_back(encoding);

        if (i < this->generationSize - 6) {
            pair = threads[j].get();
            encoding = pair.first;
            handle = pair.second;
            if (i < this->generationSize - 5 - asyncThreads) {
                k = this->selectionIndex();
                l = this->selectionIndex();
                while (k == l) {
                    l = this->selectionIndex();
                }
                threads[j] = std::async(&gen, this->generation[k].encoding, this->generation[l].encoding, i + asyncThreads, ((double) rand() / RAND_MAX) < this->mutationProbability);
            }
            j = (j + 1) % asyncThreads;
        }
    }

    this->removeAllOozebots();
    for (auto it = newGeneration.begin(); it != newGeneration.end(); it++) {
        this->insertOozebot(*it);
    }

    return this->generationSize - 5;
}

// Sort is O(N^2)
void ParetoSelector::sort() {
    std::vector<std::vector<OozebotSortWrapper>> workingVec;
    int numLeft = (int) this->generation.size();
    while (numLeft > 0) {
        std::vector<OozebotSortWrapper> nextTier;
        for (std::vector<OozebotSortWrapper>::iterator iter = this->generation.begin(); iter != this->generation.end(); iter++) {
            if ((*iter).dominationDegree == 0) {
                (*iter).dominationDegree -= 1; // invalidates it for the rest of iterations
                (*iter).novelty = this->globalParetoFront.noveltyDegreeForEncoding((*iter).encoding); // These get stale so must recompute
                nextTier.push_back(*iter);
            }
        }
        std::sort(nextTier.begin(), nextTier.end(), sortFunction);
        for (auto iter = nextTier.begin(); iter != nextTier.end(); iter++) {
            for (auto it = (*iter).dominating.begin(); it != (*iter).dominating.end(); ++it) {
                int index = this->idToIndex[(*it)];
                this->generation[index].dominationDegree -= 1;
            }
        }
        workingVec.push_back(nextTier);
        numLeft -= nextTier.size();
    }
    this->idToIndex.clear();
    std::vector<OozebotSortWrapper> nextGeneration;
    nextGeneration.reserve(this->generation.size());
    for (auto it = workingVec.begin(); it != workingVec.end(); ++it) {
        for (auto iter = (*it).begin(); iter != (*it).end(); ++iter) {
            (*iter).dominationDegree = (int) (*iter).dominated.size();
            this->idToIndex[(*iter).encoding.id] = (int) nextGeneration.size();
            nextGeneration.push_back(*iter);
            if (nextGeneration.size() == this->generationSize) {
                break;
            }
        }
    }
    this->generation = nextGeneration;
}

int ParetoSelector::selectionIndex() {
    double r = (double) rand() / RAND_MAX;

    double accumulation = this->indexToProbability[0];
    int i = 0;
    while (accumulation < r) {
        i += 1;
        accumulation += this->indexToProbability[i];
    }
    return i;
}
