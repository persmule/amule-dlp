#include <muleunit/test.h>
#include <common/StringFunctions.h>
#include <string>

using namespace muleunit;

DECLARE_SIMPLE(StringFunctions)

// Simulate one erroneous Latin-1 interpretation of the original UTF-8 bytes.
static wxString DoubleEncodeUTF8(const wxString &text)
{
	const wxCharBuffer bytes = text.utf8_str();
	return wxString(bytes.data(), wxConvISO8859_1, bytes.length());
}

TEST(StringFunctions, RepairDoubleEncodedUTF8Latin)
{
	for (const char *text : { "frías.mkv", "¡Maldita.avi" }) {
		const wxString original = wxString::FromUTF8(text);
		ASSERT_EQUALS(original, RepairDoubleEncodedUTF8(DoubleEncodeUTF8(original)));
	}
}

TEST(StringFunctions, RepairDoubleEncodedUTF8NonLatin)
{
	// Cyrillic includes C1 controls in the damaged form (e.g. К -> U+00D0 U+009A).
	for (const char *text : { "Клим.Жуков.mkv", "東京.mkv", "서울.mkv", "中国.mkv", "😀.mkv" }) {
		const wxString original = wxString::FromUTF8(text);
		ASSERT_EQUALS(original, RepairDoubleEncodedUTF8(DoubleEncodeUTF8(original)));
	}
}

TEST(StringFunctions, RepairDoubleEncodedUTF8Repeated)
{
	const wxString original = wxString::FromUTF8("Клим.frías.mkv");
	wxString damaged = original;
	for (unsigned pass = 0; pass < 4; ++pass) {
		damaged = DoubleEncodeUTF8(damaged);
		ASSERT_EQUALS(original, RepairDoubleEncodedUTF8(damaged));
	}
	// Five layers must leave one layer after the four-pass limit.
	damaged = DoubleEncodeUTF8(damaged);
	ASSERT_EQUALS(DoubleEncodeUTF8(original), RepairDoubleEncodedUTF8(damaged));
}

TEST(StringFunctions, RepairDoubleEncodedUTF8Unchanged)
{
	for (const char *text :
		{ "", "plain.ascii.mkv", "Клим.mkv", "東京.mkv", "Café.mkv", "Ñandú.mkv", "Aã£ÂÃÂ±Os" }) {
		const wxString original = wxString::FromUTF8(text);
		ASSERT_EQUALS(original, RepairDoubleEncodedUTF8(original));
	}
	// A valid sequence elsewhere must not cause a partial, lossy conversion.
	const wxString invalid = DoubleEncodeUTF8(wxString::FromUTF8("frías")) + wxString::FromUTF8("é.mkv");
	ASSERT_EQUALS(invalid, RepairDoubleEncodedUTF8(invalid));
	// Whole-name repair intentionally leaves mixed correct/damaged Unicode alone.
	const wxString mixed =
		wxString::FromUTF8("東京 ") + DoubleEncodeUTF8(wxString::FromUTF8("frías.mkv"));
	ASSERT_EQUALS(mixed, RepairDoubleEncodedUTF8(mixed));
}

TEST(StringFunctions, RepairDoubleEncodedUTF8RejectsInvalidSequences)
{
	for (const std::string &bytes : { std::string("\xC0\xAF"),
		     std::string("\xE0\x80\xAF"),
		     std::string("\xF0\x80\x80\xAF"),
		     std::string("\xED\xA0\x80"),
		     std::string("\xF4\x90\x80\x80"),
		     std::string("\xF5\x80\x80\x80"),
		     std::string("\xE6\x97") }) {
		const wxString damaged(bytes.data(), wxConvISO8859_1, bytes.size());
		ASSERT_EQUALS(damaged, RepairDoubleEncodedUTF8(damaged));
	}
}

TEST(StringFunctions, RepairDoubleEncodedUTF8KeepsLastValidPass)
{
	const wxString original = wxString::FromUTF8("frías.mkv");
	ASSERT_EQUALS(original, RepairDoubleEncodedUTF8(DoubleEncodeUTF8(original)));
	// The recovered Latin-1 text is not valid UTF-8 bytes on a subsequent pass.
	const wxString ambiguous = wxString::FromUTF8("Ã©é.mkv");
	ASSERT_EQUALS(ambiguous, RepairDoubleEncodedUTF8(DoubleEncodeUTF8(ambiguous)));
}

TEST(StringFunctions, RepairTruncatedUTF8AtEverySequenceBoundary)
{
	const wxString prefix = wxString::FromUTF8("Клим.frías.東京.");
	for (const char *character : { "К", "東", "😀" }) {
		const std::string sequence(character);
		for (size_t cut = 1; cut < sequence.size(); ++cut) {
			const std::string bytes = std::string(prefix.utf8_str()) + sequence.substr(0, cut);
			const wxString damaged(bytes.data(), wxConvISO8859_1, bytes.size());
			ASSERT_EQUALS(damaged, RepairDoubleEncodedUTF8(damaged));
			ASSERT_EQUALS(prefix, RepairDoubleEncodedUTF8(damaged, true));
			ASSERT_EQUALS(prefix, RepairSearchResultFilename(damaged));
		}
	}
}

TEST(StringFunctions, RepairTruncatedUTF8Reported160ByteNames)
{
	for (const char *text : { "oper.ru.Клим.Жуков.Столетняя.война.07.Чёрный.Принц.Орден.Подвязки."
				  "Каркассон.и.шаткий.трон.В",
		     "Разговор с Дьяволо_ Последнее предупреждение Людям Земли. Мировая Война_ Хаос_ Конец "
		     "Св" }) {
		const std::string bytes = std::string(text) + "\xD0";
		ASSERT_EQUALS(size_t(160), bytes.size());
		const wxString damaged(bytes.data(), wxConvISO8859_1, bytes.size());
		ASSERT_EQUALS(wxString::FromUTF8(text), RepairDoubleEncodedUTF8(damaged, true));
	}
}

TEST(StringFunctions, RepairTruncatedUTF8RepeatedWithinPassLimit)
{
	const wxString prefix = wxString::FromUTF8("Клим.frías.");
	const std::string bytes = std::string(prefix.utf8_str()) + "\xD0";
	const wxString truncated(bytes.data(), wxConvISO8859_1, bytes.size());
	wxString damaged = truncated;
	for (unsigned layer = 0; layer < 4; ++layer) {
		ASSERT_EQUALS(prefix, RepairDoubleEncodedUTF8(damaged, true));
		damaged = DoubleEncodeUTF8(damaged);
	}
	// Four valid passes exhaust the budget before the incomplete layer is reached.
	ASSERT_EQUALS(truncated, RepairDoubleEncodedUTF8(damaged, true));
}

TEST(StringFunctions, RepairTruncatedUTF8RequiresValidNonAsciiPrefix)
{
	for (const char *text : { "Café", "Ñandú", "plainÐ", "東京", "Ã", "é" }) {
		const wxString name = wxString::FromUTF8(text);
		ASSERT_EQUALS(name, RepairDoubleEncodedUTF8(name, true));
	}
	const std::string prefix = "name.";
	for (const std::string &tail :
		{ std::string("\xD0"), std::string("\xE6\x97"), std::string("\xF0\x9F\x98") }) {
		const std::string bytes = prefix + tail;
		const wxString damaged(bytes.data(), wxConvISO8859_1, bytes.size());
		ASSERT_EQUALS(damaged, RepairDoubleEncodedUTF8(damaged, true));
	}
}

TEST(StringFunctions, RepairTruncatedUTF8RejectsMalformedSequences)
{
	const std::string prefix = "Клим.";
	for (const std::string &tail : { std::string("\x80"),
		     std::string("\xC0"),
		     std::string("\xC1\xBF"),
		     std::string("\xE0\x80"),
		     std::string("\xED\xA0"),
		     std::string("\xF0\x80\x80"),
		     std::string("\xF4\x90"),
		     std::string("\xF5"),
		     std::string("\xD0x"),
		     std::string("\xD0x\xD0"),
		     std::string("\xFF\xD0"),
		     std::string("\xE0\x80\xAF\xD0"),
		     std::string("\xF4\x90\x80\x80\xD0") }) {
		const std::string bytes = prefix + tail;
		const wxString damaged(bytes.data(), wxConvISO8859_1, bytes.size());
		ASSERT_EQUALS(damaged, RepairDoubleEncodedUTF8(damaged, true));
	}
}

TEST(StringFunctions, RepairSearchFilenamePreservesCaseChangedEncoding)
{
	// Case changes lose information: strict decoding must not guess lead bytes.
	const wxString spanish = wxString::FromUTF8("Aã£Â") + wxUniChar(0x83) + wxString::FromUTF8("Ã") +
				 wxUniChar(0x82) + wxString::FromUTF8("Â±Os.avi");
	const wxString cyrillic =
		wxString::FromUTF8("ð") + wxUniChar(0x9A) + wxString::FromUTF8("ð»ð¸ð¼.mkv");
	for (const wxString &name : { spanish, cyrillic }) {
		ASSERT_EQUALS(name, RepairSearchResultFilename(name));
		ASSERT_EQUALS(name, RepairSearchResultFilename(DoubleEncodeUTF8(name)));
	}
}

TEST(StringFunctions, RepairSearchFilenamePreservesExistingReplacementCharacter)
{
	const wxString original = wxString::FromUTF8("Клим.�.mkv");
	ASSERT_EQUALS(original, RepairSearchResultFilename(original));
	ASSERT_EQUALS(original, RepairSearchResultFilename(DoubleEncodeUTF8(original)));
}

TEST(StringFunctions, RepairTruncatedUTF8ContinuesDecodingPrefix)
{
	// Dropping a tail must not stop a later strict pass over a Latin-1 prefix.
	const wxString original = wxString::FromUTF8("frías.mkv");
	const std::string bytes = std::string(DoubleEncodeUTF8(original).utf8_str()) + "\xD0";
	const wxString damaged(bytes.data(), wxConvISO8859_1, bytes.size());
	ASSERT_EQUALS(original, RepairSearchResultFilename(damaged));
}

///////////////////////////////////////////////////////////
// Tests for the FuzzyStrCmp function

//! Returns the number of items in an array.
#define itemsof(x) (sizeof(x) / sizeof(x[0]))

TEST(StringFunctions, FuzzyStrCmp)
{
	struct FuzzyTest
	{
		const char *a;
		const char *b;
		int expected;
	};

	FuzzyTest checks[] = {
		{ "a (10)", "a (2)", 1 },
		{ "a (10)", "b (2)", -1 },
		{ "c3 (7)", "c3 (12)", -1 },
		{ "c3 (12)", "c3 (7)", 1 },
		{ "c3 12d", "c3 12d", 0 },
		{ "a (10)  ", "a (2)  ", 1 },
		{ "a (10)  ", "b (2)  ", -1 },
		{ "  c3 (7)", "  c3 (12)", -1 },
		{ "  c3 (12)", "  c3 (7)", 1 },
		{ "c3 12d", "c3 12d", 0 },
		{ "", "", 0 },
		{ "", "c3 12d", -1 },
		{ "c3 12d", "", 1 },
		{ " ", "c3 12d", -1 },
		{ "c3 12d", " ", 1 },
		{ "17.10", "17.2", 1 },
		{ "  c3 (a)", "  c3 (12)", 1 },
		{ "  c3 (12)", "  c3 (a)", -1 },
	};

	for (size_t i = 0; i < itemsof(checks); ++i) {
		wxString a = checks[i].a;
		wxString b = checks[i].b;

		ASSERT_EQUALS(checks[i].expected, FuzzyStrCmp(a, b));
	}
}

///////////////////////////////////////////////////////////
// Tests for the CSimpleParser class

DECLARE_SIMPLE(SimpleParser)

TEST(SimpleParser, Constructor)
{
	// Empty strings are acceptable and should just return an empty string
	{
		CSimpleTokenizer tkz1("", '-');
		ASSERT_EQUALS("", tkz1.remaining());
		ASSERT_EQUALS("", tkz1.next());
		ASSERT_EQUALS("", tkz1.remaining());
		ASSERT_EQUALS("", tkz1.next());
	}

	// String with no tokens should be return immediately
	{
		CSimpleTokenizer tkz2(" abc ", '-');
		ASSERT_EQUALS(" abc ", tkz2.remaining());
		ASSERT_EQUALS(" abc ", tkz2.next());
		ASSERT_EQUALS("", tkz2.next());
		ASSERT_EQUALS("", tkz2.next());
	}
}

TEST(SimpleParser, EmptyTokens)
{
	{
		CSimpleTokenizer tkz1(" a", ' ');
		ASSERT_EQUALS(" a", tkz1.remaining());
		ASSERT_EQUALS(0u, tkz1.tokenCount());

		ASSERT_EQUALS("", tkz1.next());
		ASSERT_EQUALS("a", tkz1.remaining());
		ASSERT_EQUALS(1u, tkz1.tokenCount());

		ASSERT_EQUALS("a", tkz1.next());
		ASSERT_EQUALS("", tkz1.remaining());
		ASSERT_EQUALS(1u, tkz1.tokenCount());

		ASSERT_EQUALS("", tkz1.next());
		ASSERT_EQUALS("", tkz1.remaining());
		ASSERT_EQUALS(1u, tkz1.tokenCount());
	}

	{
		CSimpleTokenizer tkz2("c ", ' ');
		ASSERT_EQUALS("c ", tkz2.remaining());
		ASSERT_EQUALS(0u, tkz2.tokenCount());

		ASSERT_EQUALS("c", tkz2.next());
		ASSERT_EQUALS("", tkz2.remaining());
		ASSERT_EQUALS(1u, tkz2.tokenCount());

		ASSERT_EQUALS("", tkz2.next());
		ASSERT_EQUALS("", tkz2.remaining());
		ASSERT_EQUALS(1u, tkz2.tokenCount());

		ASSERT_EQUALS("", tkz2.next());
		ASSERT_EQUALS("", tkz2.remaining());
		ASSERT_EQUALS(1u, tkz2.tokenCount());
	}

	{
		CSimpleTokenizer tkz3(" a c ", ' ');
		ASSERT_EQUALS(" a c ", tkz3.remaining());
		ASSERT_EQUALS(0u, tkz3.tokenCount());

		ASSERT_EQUALS("", tkz3.next());
		ASSERT_EQUALS("a c ", tkz3.remaining());
		ASSERT_EQUALS(1u, tkz3.tokenCount());

		ASSERT_EQUALS("a", tkz3.next());
		ASSERT_EQUALS("c ", tkz3.remaining());
		ASSERT_EQUALS(2u, tkz3.tokenCount());

		ASSERT_EQUALS("c", tkz3.next());
		ASSERT_EQUALS("", tkz3.remaining());
		ASSERT_EQUALS(3u, tkz3.tokenCount());

		ASSERT_EQUALS("", tkz3.next());
		ASSERT_EQUALS("", tkz3.remaining());
		ASSERT_EQUALS(3u, tkz3.tokenCount());

		ASSERT_EQUALS("", tkz3.next());
		ASSERT_EQUALS("", tkz3.remaining());
		ASSERT_EQUALS(3u, tkz3.tokenCount());
	}
}

TEST(SimpleParser, NormalTokens)
{
	CSimpleTokenizer tkz("a c", ' ');
	ASSERT_EQUALS("a c", tkz.remaining());
	ASSERT_EQUALS(0u, tkz.tokenCount());

	ASSERT_EQUALS("a", tkz.next());
	ASSERT_EQUALS("c", tkz.remaining());
	ASSERT_EQUALS(1u, tkz.tokenCount());

	ASSERT_EQUALS("c", tkz.next());
	ASSERT_EQUALS("", tkz.remaining());
	ASSERT_EQUALS(1u, tkz.tokenCount());

	ASSERT_EQUALS("", tkz.next());
	ASSERT_EQUALS("", tkz.remaining());
	ASSERT_EQUALS(1u, tkz.tokenCount());

	ASSERT_EQUALS("", tkz.next());
	ASSERT_EQUALS("", tkz.remaining());
	ASSERT_EQUALS(1u, tkz.tokenCount());
}

///////////////////////////////////////////////////////////
// Tests for UnescapeHTML

TEST(StringFunctions, UnescapeHTMLPlainAscii)
{
	ASSERT_EQUALS("some file name.avi", UnescapeHTML("some%20file%20name.avi"));
	ASSERT_EQUALS("a|b", UnescapeHTML("a%7Cb"));
	// An escape that is not two hex digits is left exactly as it stands.
	ASSERT_EQUALS("100%25 sure", UnescapeHTML("100%2525%20sure"));
	ASSERT_EQUALS("50% off", UnescapeHTML("50%%20off"));
}

// Regression: the escapes were located by byte offset in a UTF-8 buffer but read by character
// offset out of the wxString, so one multi-byte character desynchronised the two and every
// following escape decoded from the wrong place -- injecting stray bytes (a newline, here) and
// leaving the real escapes untouched. An eD2k link whose filename had an accent came out of
// CED2KFileLink split in half.
TEST(StringFunctions, UnescapeHTMLAfterNonAsciiCharacter)
{
	const wxString escaped =
		wxString::FromUTF8("Une%20beaut\xC3\xA9%20faite%20au%20naturel%20-%20Part%202.mkv");
	const wxString expected = wxString::FromUTF8("Une beaut\xC3\xA9 faite au naturel - Part 2.mkv");

	ASSERT_EQUALS(expected, UnescapeHTML(escaped));
}

TEST(StringFunctions, UnescapeHTMLKeepsNonAsciiWithoutEscapes)
{
	const wxString name = wxString::FromUTF8("caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC.mkv");

	ASSERT_EQUALS(name, UnescapeHTML(name));
}

// A '%' whose next two bytes are not hex digits at all -- here the start of a UTF-8 sequence -- has
// to survive as a literal, without tripping a debug assertion on the way.
TEST(StringFunctions, UnescapeHTMLPercentBeforeNonAscii)
{
	const wxString name = wxString::FromUTF8("100%\xC3\xA9.mkv");

	ASSERT_EQUALS(name, UnescapeHTML(name));
}

// Issue #873: Chromium refuses to open an ed2k:// URL holding a literal '|', so links are published
// and copied with the delimiters percent-encoded, and CED2KLink::CreateLinkFromUrl retries through
// this helper.
TEST(StringFunctions, RestoreEncodedPipesHandlesBothCases)
{
	ASSERT_EQUALS(wxString("ed2k://|file|a.iso|1|H|/"),
		RestoreEncodedPipes("ed2k://%7Cfile%7Ca.iso%7C1%7CH%7C/"));
	ASSERT_EQUALS(wxString("ed2k://|file|a.iso|1|H|/"),
		RestoreEncodedPipes("ed2k://%7cfile%7ca.iso%7c1%7cH%7c/"));
}

// A link that a mail client only half-encoded still has to come out whole.
TEST(StringFunctions, RestoreEncodedPipesHandlesMixedSpellings)
{
	ASSERT_EQUALS(wxString("a|b|c"), RestoreEncodedPipes("a%7Cb|c"));
}

// Only the delimiters are its business: every other escape belongs to the filename, which
// UnescapeHTML decodes later, and decoding it here would double-decode a name containing a literal
// '%'.
TEST(StringFunctions, RestoreEncodedPipesLeavesOtherEscapesAlone)
{
	ASSERT_EQUALS(wxString("a|b%20c%2F"), RestoreEncodedPipes("a%7Cb%20c%2F"));
	ASSERT_EQUALS(wxString("nothing to do"), RestoreEncodedPipes("nothing to do"));
	ASSERT_EQUALS(wxEmptyString, RestoreEncodedPipes(wxEmptyString));
}

// Every string contains "", so an empty entry from a doubled, leading or blank-only comma used to
// match everything: one stray comma in a filter list dropped every message.
TEST(StringFunctions, ContainsAnyKeywordSkipsEmptyEntries)
{
	const wxString text("hello friend");
	ASSERT_FALSE(ContainsAnyKeyword(text, "spam,viagra"));
	ASSERT_FALSE(ContainsAnyKeyword(text, "spam,,viagra"));
	ASSERT_FALSE(ContainsAnyKeyword(text, "spam, ,viagra"));
	ASSERT_FALSE(ContainsAnyKeyword(text, ",spam"));
	ASSERT_FALSE(ContainsAnyKeyword(text, "spam,"));
	ASSERT_FALSE(ContainsAnyKeyword(text, " "));
	ASSERT_FALSE(ContainsAnyKeyword(text, wxEmptyString));
}

TEST(StringFunctions, ContainsAnyKeywordMatchesTrimmedAndCaseInsensitive)
{
	ASSERT_TRUE(ContainsAnyKeyword("Buy VIAGRA now", "spam,,viagra"));
	ASSERT_TRUE(ContainsAnyKeyword("buy now please", " spam , Buy Now "));
	ASSERT_FALSE(ContainsAnyKeyword(wxEmptyString, "spam"));
}
