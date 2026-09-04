#include "speciesStream.hh"

#include <cstring>

using namespace std;

namespace {

/* Extract the element name from a raw tag such as `<Species id="S1">` or
 * `</Species>`.  Returns "/Species" for a close tag so callers can switch on
 * a single string. */
string tagName(const string &tag)
{
	size_t i = 1;                       // skip '<'
	if (i < tag.size() && tag[i] == '?') return "?";
	if (i < tag.size() && tag[i] == '!') return "!";
	string name;
	if (i < tag.size() && tag[i] == '/') { name += '/'; ++i; }
	for (; i < tag.size(); ++i) {
		char c = tag[i];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
				c == '>' || c == '/') break;
		name += c;
	}
	return name;
}

bool isSelfClosing(const string &tag)
{
	// walk back from the '>' past whitespace looking for '/'
	size_t i = tag.size();
	while (i > 0 && (tag[i-1] == '>' || tag[i-1] == ' ' || tag[i-1] == '\t' ||
			tag[i-1] == '\n' || tag[i-1] == '\r')) {
		if (tag[i-1] == '>') { --i; break; }
		--i;
	}
	while (i > 0 && (tag[i-1] == ' ' || tag[i-1] == '\t' ||
			tag[i-1] == '\n' || tag[i-1] == '\r')) --i;
	return i > 0 && tag[i-1] == '/';
}

}

namespace NFinput {

bool splitImpl(const string &filename, string &skeleton,
		vector<SpeciesSpan> *speciesSpans, vector<SpeciesSpan> *ruleSpans);

bool splitStreamedSections(const string &filename, string &skeleton,
		vector<SpeciesSpan> &speciesSpans, vector<SpeciesSpan> &ruleSpans)
{
	return splitImpl(filename, skeleton, &speciesSpans, &ruleSpans);
}

bool splitSpeciesStream(const string &filename, string &skeleton,
		vector<SpeciesSpan> &spans)
{
	vector<SpeciesSpan> unusedRules;
	return splitImpl(filename, skeleton, &spans, 0);
}

bool splitImpl(const string &filename, string &skeleton,
		vector<SpeciesSpan> *speciesSpans, vector<SpeciesSpan> *ruleSpans)
{
	skeleton.clear();
	if (speciesSpans) speciesSpans->clear();
	if (ruleSpans) ruleSpans->clear();

	FILE *in = fopen(filename.c_str(), "rb");
	if (in == 0) return false;

	const size_t BUFSZ = 1 << 16;
	vector<char> buffer(BUFSZ);

	string tag;                 // current tag text, only ever a few hundred bytes
	bool inTag = false;
	char quote = 0;             // active attribute quote character, 0 if none

	/* Which container we are currently inside, and which child element name
	 * we are eliding within it. 0 = none. */
	const char *activeChild = 0;
	vector<SpeciesSpan> *activeOut = 0;
	int speciesDepth = 0;       // >0 while inside an elided child element
	long long speciesStart = 0;
	bool sawSpeciesList = false;

	long long offset = 0;       // file offset of the next byte to be read
	size_t got = 0;

	/* Reserve enough for a typical non-species remainder so the skeleton does
	 * not repeatedly reallocate on large models. */
	skeleton.reserve(1 << 20);

	while ((got = fread(&buffer[0], 1, BUFSZ, in)) > 0) {
		for (size_t k = 0; k < got; ++k, ++offset) {
			char c = buffer[k];

			if (!inTag) {
				if (c == '<') {
					inTag = true;
					quote = 0;
					tag.assign(1, c);
					// remember where this tag began in case it opens a Species
					if (speciesDepth == 0) speciesStart = offset;
				} else if (speciesDepth == 0) {
					// character data outside any Species element
					skeleton += c;
				}
				continue;
			}

			// inside a tag
			tag += c;
			if (quote != 0) {
				if (c == quote) quote = 0;
				continue;
			}
			if (c == '"' || c == '\'') { quote = c; continue; }
			if (c != '>') continue;

			// tag is complete
			inTag = false;
			const string name = tagName(tag);
			const long long tagEnd = offset;   // offset of '>'

			if (speciesDepth > 0) {
				// suppressing output; track nesting so we find the right close
				if (name == activeChild && !isSelfClosing(tag)) {
					++speciesDepth;
				} else if (name.size() > 1 && name[0] == '/' &&
						name.compare(1, string::npos, activeChild) == 0) {
					if (--speciesDepth == 0) {
						SpeciesSpan sp;
						sp.start = speciesStart;
						sp.length = tagEnd - speciesStart + 1;
						activeOut->push_back(sp);
					}
				}
				continue;
			}

			if (name == "ListOfSpecies" && speciesSpans) {
				sawSpeciesList = true;
				if (!isSelfClosing(tag)) {
					activeChild = "Species"; activeOut = speciesSpans;
				}
				skeleton += tag;
				continue;
			}
			if (name == "ListOfReactionRules" && ruleSpans) {
				if (!isSelfClosing(tag)) {
					activeChild = "ReactionRule"; activeOut = ruleSpans;
				}
				skeleton += tag;
				continue;
			}
			if (name == "ListOfSpecies") { sawSpeciesList = true; skeleton += tag; continue; }
			if (name == "/ListOfSpecies" || name == "/ListOfReactionRules") {
				activeChild = 0; activeOut = 0;
				skeleton += tag;
				continue;
			}
			if (activeChild != 0 && name == activeChild) {
				if (isSelfClosing(tag)) {
					SpeciesSpan sp;
					sp.start = speciesStart;
					sp.length = tagEnd - speciesStart + 1;
					activeOut->push_back(sp);
				} else {
					speciesDepth = 1;      // speciesStart already recorded
				}
				continue;
			}
			skeleton += tag;
		}
	}
	fclose(in);

	// An unterminated tag or unbalanced Species element means we cannot trust
	// the split; let the caller fall back to the DOM reader.
	if (inTag || speciesDepth != 0 || !sawSpeciesList) {
		skeleton.clear();
		if (speciesSpans) speciesSpans->clear();
		if (ruleSpans) ruleSpans->clear();
		return false;
	}
	return true;
}

bool readSpeciesSpan(FILE *handle, const SpeciesSpan &span, string &fragment)
{
	if (handle == 0 || span.length <= 0) return false;
	if (fseek(handle, (long) span.start, SEEK_SET) != 0) return false;
	fragment.resize((size_t) span.length);
	size_t got = fread(&fragment[0], 1, (size_t) span.length, handle);
	return got == (size_t) span.length;
}

}
