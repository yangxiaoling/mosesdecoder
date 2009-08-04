#include "StaticData.h"
#include "WordLattice.h"
#include "PCNTools.h"
#include "Util.h"
#include "FloydWarshall.h"

namespace Moses
{
WordLattice::WordLattice() {}

size_t WordLattice::GetColumnIncrement(size_t i, size_t j) const
{
	return next_nodes[i][j];
}

void WordLattice::Print(std::ostream& out) const {
	out<<"word lattice: "<<data.size()<<"\n";
	for(size_t i=0;i<data.size();++i) {
		out<<i<<" -- ";
		for(size_t j=0;j<data[i].size();++j)
			out<<"("<<data[i][j].first.ToString()<<", "<<data[i][j].second<<", " << GetColumnIncrement(i,j) << ") ";
		out<<"\n";
	}
	out<<"\n\n";
}

int WordLattice::Read(std::istream& in,const std::vector<FactorType>& factorOrder)
{
	Clear();
	std::string line;
	if(!getline(in,line)) return 0;
	std::map<std::string, std::string> meta=ProcessAndStripSGML(line);
	if (meta.find("id") != meta.end()) { this->SetTranslationId(atol(meta["id"].c_str())); }
	PCN::CN cn = PCN::parsePCN(line);
	data.resize(cn.size());
	next_nodes.resize(cn.size());
	for(size_t i=0;i<cn.size();++i) {
		PCN::CNCol& col = cn[i];
		if (col.empty()) return false;
		data[i].resize(col.size());
		next_nodes[i].resize(col.size());
		for (size_t j=0;j<col.size();++j) {
			PCN::CNAlt& alt = col[j];
			if (alt.first.second < 0.0f) { TRACE_ERR("WARN: neg probability: " << alt.first.second); alt.first.second = 0.0f; }
			if (alt.first.second > 1.0f) { TRACE_ERR("WARN: probability > 1: " << alt.first.second); alt.first.second = 1.0f; }
			data[i][j].second = std::max(static_cast<float>(log(alt.first.second)), LOWEST_SCORE);
			//data[i][j].second = std::max(static_cast<float>((alt.first.second)), LOWEST_SCORE);
			String2Word(alt.first.first,data[i][j].first,factorOrder);
			next_nodes[i][j] = alt.second;
		}
	}
	if (!cn.empty()) {
		std::vector<std::vector<bool> > edges(0);
		this->GetAsEdgeMatrix(edges);
		floyd_warshall(edges,distances);

		IFVERBOSE(2) {
			TRACE_ERR("Shortest paths:\n");
			for (size_t i=0; i<edges.size(); ++i) {
				for (size_t j=0; j<edges.size(); ++j) {
					int d = distances[i][j];
					if (d > 99999) { d=-1; }
					TRACE_ERR("\t" << d);
				}
				TRACE_ERR("\n");
			}
		}
	}
	return !cn.empty();
}

void WordLattice::GetAsEdgeMatrix(std::vector<std::vector<bool> >& edges) const
{
  edges.resize(data.size()+1,std::vector<bool>(data.size()+1, false));
  for (size_t i=0;i<data.size();++i) {
    for (size_t j=0;j<data[i].size(); ++j) {
      edges[i][i+next_nodes[i][j]] = true;
    }
  }
}

int WordLattice::ComputeDistortionDistance(const WordsRange& prev, const WordsRange& current) const
{

#if 1
	int result;
	if (prev.GetStartPos() == NOT_FOUND) {
		//TRACE_ERR("returning initial distance from 0 to " << (current.GetStartPos()+1) << " which is " << (distances[0][current.GetStartPos()+1] - 1) <<"\n");
		result = distances[0][current.GetStartPos()+1] - 1;
		if (result < 0 || result > 99999) {
			TRACE_ERR("prev: " << prev << "\n current: " << current << "\n");
			TRACE_ERR("A: got a weird distance from 0 to " << (current.GetStartPos()+1) << " of " << result << "\n");
		}
	} else if (prev.GetEndPos() > current.GetStartPos()) {
		//TRACE_ERR("returning forward distance from "<< current.GetStartPos() << " to " << (prev.GetEndPos()+1) << " which is " << distances[current.GetStartPos()][prev.GetEndPos()+1] <<"\n");
		result = distances[current.GetStartPos()][prev.GetEndPos()+1];
		if (result < 0 || result > 99999) {
			TRACE_ERR("prev: " << prev << "\n current: " << current << "\n");

			TRACE_ERR("B: got a weird distance from "<< current.GetStartPos() << " to " << prev.GetEndPos()+1 << " of " << result << "\n");
		}
	} else if (prev.GetEndPos()+1 == current.GetStartPos()) {
		return 0;
	} else {
		//TRACE_ERR("returning reverse distance from "<< (prev.GetEndPos()+1) << " to " << (current.GetStartPos()+1) << " which is " << (distances[prev.GetEndPos()+1][current.GetStartPos()+1] - 1) <<"\n");
		result = distances[prev.GetEndPos() + 1][current.GetStartPos()] - 1;
		if (result < 0 || result > 99999) {
			TRACE_ERR("prev: " << prev << "\n current: " << current << "\n");

			TRACE_ERR("C: got a weird distance from "<< prev.GetEndPos()+1 << " to " << current.GetStartPos() << " of " << result << "\n");
		}
	}
	return result;
#else
	int dist = 0;
	if (prev.GetNumWordsCovered() == 0) {
		dist = current.GetStartPos();
  } else {
	  dist = (int)prev.GetEndPos() - (int)current.GetStartPos() + 1 ;
  }
	return abs(dist);
#endif
}

bool WordLattice::CanIGetFromAToB(size_t start, size_t end) const
{
	//  std::cerr << "CanIgetFromAToB(" << start << "," << end << ")=" << distances[start][end] << std::endl;
	return distances[start][end] < 100000;
}


}

