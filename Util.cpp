#include <cassert>
#include <cstring>
#ifndef _USE_MATH_DEFINES
	#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <cctype> // for isspace

#include "headers/HAIInterface.h"
#include "Util.hpp"

namespace util {
	std::string GetAbsFileName(IAICallback* cb, const std::string& relFileName, bool readonly) {
		// this size equals the one used in "AIAICallback::GetValue(AIVAL_LOCATE_FILE_..."
		char dst[2048];

		SNPRINTF(dst, sizeof(dst), "%s", relFileName.c_str());

		// get the absolute path to the file (and create folders along the way)
		if (readonly) {
			cb->GetValue(AIVAL_LOCATE_FILE_R, dst);
		} else {
			cb->GetValue(AIVAL_LOCATE_FILE_W, dst);
		}

		return (std::string(dst));
	}

	void SanitizeFileNameInPlace(std::string& s) {
		static const char chars2replace[] = "<>:?| \t\n\r";
		size_t pos = 0;
		while((pos = s.find_first_of(chars2replace, pos)) != std::string::npos) {
			s[pos] = '_';
			pos++;
		}
	}

	void StringToLowerInPlace(std::string& s) {
		std::transform(s.begin(), s.end(), s.begin(), (int (*)(int))tolower);
	}

	std::string StringToLower(std::string s) {
		StringToLowerInPlace(s);
		return s;
	}

	std::string StringStripSpaces(const std::string& s1) {
		std::string s2; s2.reserve(s1.size());

		for (std::string::const_iterator it = s1.begin(); it != s1.end(); ++it) {
			if (!isspace(*it)) {
				s2.push_back(*it);
			}
		}

		return s2;
	}

	void StringSplit(const std::string& src, char sep, std::vector<std::string>& dest, bool empty) {
		bool go = true;
		size_t pos1 = 0, pos2;

		if (empty) dest.clear();
		if (src.empty()) return;

		while (go) {
			pos2 = src.find(sep, pos1);
			if (pos2 == std::string::npos) {
				pos2 = src.size();
				go = false;
			}
			dest.push_back(src.substr(pos1, pos2 - pos1));
			pos1 = pos2 + 1;
		}
	}

	void RemoveWhiteSpaceInPlace(std::string& s) {
		s.erase(remove_if(s.begin(), s.end(), isspace), s.end());
	}

	bool StringContainsChar(const std::string& src, const char c) {
		for (int i = 0; i < src.length(); i++ ) {
			if (src[i] == c)
				return true;
		}
		return false;
	}

	std::string IntToString(int i, const std::string& format) {
		char buf[64];
		SNPRINTF(buf, sizeof(buf), format.c_str(), i);
		return std::string(buf);
	}

	float WeightedAverage(std::list<float>& V, std::list<float>& W) {
		float wavg = 0.0f;
		std::list<float>::const_iterator v, w;
		for (w = W.begin(), v = V.begin(); v != V.end() && w != W.end(); ++w, ++v)
			wavg += ((*w) * (*v));

		return wavg;
	}

	bool IsBinarySubset(unsigned A, unsigned B) {
		unsigned cA     = CountOneBits(A);
		unsigned cAandB = CountOneBits(A&B);

		return (cA == cAandB);
	}

	unsigned int CountOneBits(unsigned int n) {
		const int S[] = {1, 2, 4, 8, 16};
		const int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};
		int c = n;
		c = ((c >> S[0]) & B[0]) + (c & B[0]);
		c = ((c >> S[1]) & B[1]) + (c & B[1]);
		c = ((c >> S[2]) & B[2]) + (c & B[2]);
		c = ((c >> S[3]) & B[3]) + (c & B[3]);
		c = ((c >> S[4]) & B[4]) + (c & B[4]);
		return c;
	}

	float GaussDens(float x, float mu, float sigma) {
		const float a = 1.0f / (sigma * std::sqrt(2.0f * M_PI));
		const float b = std::exp(-(((x - mu) * (x - mu)) / (2.0f * sigma * sigma)));
		return (a * b);
	}
}
