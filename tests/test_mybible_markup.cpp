// Spec tests for MyBibleToOsis (src/core/MyBibleMarkup.h). Written by Claude. Every expected
// string here follows the mapping in the header exactly; don't change them.
#include "check.h"
#include "MyBibleMarkup.h"

using namespace sheath;

static void t(const std::string& in, bool nt, const std::string& want, const std::string& name)
{
	check_eq(MyBibleToOsis(in, nt, nullptr), want, name);
}

int main()
{
	// Plain text and escaping
	t("In the beginning God created the heaven and the earth.", false,
		"In the beginning God created the heaven and the earth.", "plain text unchanged");
	t("Jesus & Mary", true, "Jesus &amp; Mary", "bare & escaped");
	t("a &amp; b &nbsp; c &#33; d &#x21;", true, "a &amp; b &nbsp; c &#33; d &#x21;", "entities kept");
	t("if a < b and c > d", true, "if a &lt; b and c &gt; d", "< and > outside tags escaped");
	t("", true, "", "empty");

	// Simple tags
	t("<J>I am the way</J>, said he", true,
		"<q who=\"Jesus\" marker=\"\">I am the way</q>, said he", "J -> q who=Jesus");
	t("The LORD <i>is</i> my shepherd", false,
		"The LORD <transChange type=\"added\">is</transChange> my shepherd", "i -> transChange added");
	t("he <e>went</e> away", true, "he <hi type=\"emphasis\">went</hi> away", "e -> hi emphasis");
	t("the Word<f>[1] Or, reason</f>, and", true,
		"the Word<note type=\"x-footnote\">[1] Or, reason</note>, and", "f -> note");
	t("text<n>a note</n> more<m>V-PAI-3S</m>.", true, "text more.", "n and m removed with content");
	t("<t>The LORD is my shepherd;</t><t>I shall not want.</t>", false,
		"The LORD is my shepherd;<lb/>I shall not want.<lb/>", "t -> line + lb");
	t("<pb/>And it came to pass", false, "<milestone type=\"x-p\"/>And it came to pass", "pb -> milestone");
	t("Cana<br/>in Galilee", true, "Cana<lb/>in Galilee", "br -> lb");

	// Strong's numbers
	t("God so loved<S>25</S> the world", true,
		"God so <w lemma=\"strong:G25\">loved</w> the world", "S: word before it, NT -> G");
	t("In the beginning<S>7225</S> God<S>430</S> created", false,
		"In the <w lemma=\"strong:H7225\">beginning</w> <w lemma=\"strong:H430\">God</w> created",
		"S: OT -> H, two words");
	t("the Son<S>3588</S><S>5207</S>, that", true,
		"the <w lemma=\"strong:G3588 strong:G5207\">Son</w>, that", "S: several numbers, one lemma");
	t("of God,<S>2316</S> and", true, "of <w lemma=\"strong:G2316\">God</w>, and",
		"S: trailing punctuation stays outside");
	t("God<S>H0430</S> said", false, "<w lemma=\"strong:H0430\">God</w> said", "S: prefix kept");
	t("<S>3588</S>For<S>1063</S> God", true,
		"<w lemma=\"strong:G3588\"></w><w lemma=\"strong:G1063\">For</w> God", "S: no word before -> empty w");
	t("<J>loved<S>25</S> you</J>", true,
		"<q who=\"Jesus\" marker=\"\"><w lemma=\"strong:G25\">loved</w> you</q>", "S: word inside J");
	t("<i>is</i><S>1510</S> come", true,
		"<transChange type=\"added\">is</transChange><w lemma=\"strong:G1510\"></w> come",
		"S: right after a tag -> empty w");
	t("Ἰησοῦς<S>2424</S> wept", true, "<w lemma=\"strong:G2424\">Ἰησοῦς</w> wept", "S: UTF-8 word");

	// Robustness
	t("<x>unknown</x> tag", true, "unknown tag", "unknown tag dropped, content kept");
	t("stray</i> close", true, "stray close", "stray closing tag dropped");
	t("<J>never closed", true, "<q who=\"Jesus\" marker=\"\">never closed</q>", "open tag closed at end");
	t("<f>note <i>with</i> added</f>", true,
		"<note type=\"x-footnote\">note <transChange type=\"added\">with</transChange> added</note>",
		"nested tags converted");

	// Flags
	OsisFlags f;
	MyBibleToOsis("plain", true, &f);
	check_true(!f.strongs && !f.redLetter && !f.footnotes, "flags: none for plain text");
	MyBibleToOsis("<J>a</J>", true, &f);
	check_true(f.redLetter && !f.strongs && !f.footnotes, "flags: redLetter");
	MyBibleToOsis("a<S>1</S>", true, &f);
	MyBibleToOsis("b<f>n</f>", true, &f);
	check_true(f.redLetter && f.strongs && f.footnotes, "flags: accumulate, never reset");
	return check_result();
}
