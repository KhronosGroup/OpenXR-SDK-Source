#!/usr/bin/env python3
# Copyright (c) 2019-2023 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Rylie Pavlik <rylie.pavlik@collabora.com>
#
# Purpose:      This script helps drive a per-section PDF diff.

import re
from itertools import chain, zip_longest
from pathlib import Path
from pprint import pprint

import attr
from PyPDF2 import PdfReader

from pdf_diff import command_line as pdf_diff

NUMBERED_TITLE_RE = re.compile(
    r'(?P<section_num>([0-9]+[.])+) (?P<title_text>.*)')


@attr.s
class Bookmark:
    title = attr.ib()
    level = attr.ib()
    page_number = attr.ib()
    top = attr.ib(default=None)
    bottom = attr.ib(default=None)
    left = attr.ib(default=None)
    right = attr.ib(default=None)
    nested_title = attr.ib(default=None)

    def asdict(self):
        # fields = attr.fields(Section)
        return attr.asdict(self)


@attr.s
class Section:
    title = attr.ib()
    level = attr.ib()
    page_start = attr.ib()
    page_end = attr.ib()
    pdf = attr.ib(default=None)
    page_start_top = attr.ib(default=None)
    page_end_bottom = attr.ib(default=None)

    @property
    def page_numbers(self):
        return range(self.page_start, self.page_end + 1)

    @property
    def section_number(self):
        match = NUMBERED_TITLE_RE.match(self.title)
        if match:
            return match.group('section_num')
        return None

    @property
    def title_text(self):
        match = NUMBERED_TITLE_RE.match(self.title)
        if match:
            return match.group('title_text')
        return self.title

    @property
    def pdf_diff_options(self):
        fields = attr.fields(Section)
        ret = attr.asdict(self, filter=attr.filters.include(
            fields.page_start, fields.page_end))
        ret['fn'] = str(self.pdf.fn)
        if self.page_start_top:
            ret['page_start_top'] = self.page_start_top

        if self.page_end_bottom:
            ret['page_end_bottom'] = self.page_end_bottom
        return ret

    def asdict(self):
        return attr.asdict(self,
                           filter=attr.filters.exclude(
                               attr.fields(Section).pdf))


def add_nested_title(bookmarks):
    title_stack = []
    for i, bookmark in enumerate(bookmarks):
        while bookmark.level >= len(title_stack):
            title_stack.pop()
        title_stack.append(bookmark.title)
        bookmark.nested_title = " : ".join(title_stack)


def outline_to_bookmarks(reader, outline=None, level=1, bookmarks=None):
    if outline is None:
        outline = reader.outline
    if bookmarks is None:
        bookmarks = []
    for elt in outline:
        if isinstance(elt, list):
            outline_to_bookmarks(reader, outline=elt, level=level+1,
                                 bookmarks=bookmarks)
        else:
            page_num = reader.get_destination_page_number(elt)
            bookmark = Bookmark(
                title=elt.title,
                level=level,
                page_number=page_num + 1)
            page = reader.pages[page_num]
            _, ul_y = page.bleedbox.upper_left
            # print(page.bleedbox.upper_left, page.bleedbox.lower_right)
            # Coordinate system is flipped compared to pdf_diff...
            if elt.top:
                bookmark.top = float(ul_y) - float(elt.top)
            if elt.bottom:
                bookmark.bottom = float(ul_y) - float(elt.bottom)
            if elt.left:
                bookmark.left = float(elt.left)
            if elt.right:
                bookmark.right = float(elt.right)
            bookmarks.append(bookmark)
    return bookmarks


def add_section_to_page_map(section, page_map):
    for i in section.page_numbers:
        if i not in page_map:
            page_map[i] = []
        page_map[i].append(section)


def compute_section_page_map(sections):
    page_map = {}
    for sec in sections:
        # +1 is for an inclusive range
        for page_num in sec.page_numbers:
            if page_num not in page_map:
                page_map[page_num] = []
            page_map[page_num].append(sec)
    return page_map


class PdfSpec:
    def __init__(self, fn):
        self.fn = fn
        self.reader = PdfReader(open(str(fn), 'rb'))
        self.bookmark_data = outline_to_bookmarks(self.reader)

        self._page_pdfs = None

        self._dom = None

    @property
    def dom(self):
        if self._dom is None:
            self._dom = pdf_diff.pdf_to_dom(str(self.fn))
        return self._dom

    @property
    def page_pdfs(self):
        import pypdftk
        if not self._page_pdfs:
            self._page_pdfs = pypdftk.split(self.fn)
        return self._page_pdfs

    def pdf_for_page(self, pagenum):
        if pagenum is None:
            return None
        return self.page_pdfs[pagenum]

    def compute_sections(self,
                         level=None,
                         bookmarks=None,
                         bookmark_predicate=None):
        if level is None:
            level = 1
        if bookmarks is None:
            if bookmark_predicate is not None:
                bookmarks = [x for x in self.bookmark_data
                             if bookmark_predicate(x)]
            else:
                bookmarks = [x for x in self.bookmark_data
                             if x.level == level]

        sections = []
        # Add a placeholder section taking up all front-matter pages
        first_bookmark = bookmarks[0]
        if first_bookmark.page_number != 1:
            sections.append(Section(title="0. Front Matter",
                                    level=level,
                                    page_start=1,
                                    page_end=first_bookmark.page_number,
                                    pdf=self))

        prev_bookmark = None
        for i, bookmark in enumerate(bookmarks):
            if prev_bookmark is not None:
                s = prev_bookmark.page_number
                e = bookmark.page_number
                if not bookmark.top or bookmark.top == 0:
                    # If no "top", then assume the section ends on a page break.
                    e -= 1
                sec = Section(title=prev_bookmark.title,
                              level=prev_bookmark.level,
                              page_start=s,
                              page_end=e,
                              pdf=self)
                if prev_bookmark.top:
                    sec.page_start_top = prev_bookmark.top
                if bookmark.top:
                    sec.page_end_bottom = bookmark.top
                sections.append(sec)
            prev_bookmark = bookmark
        if bookmark is not None:
            # TODO Deal with the last section here!
            pass

        # Now, populate the object fields
        self.comparable_sections = sections
        self.section_by_title = {sec.title: sec
                                 for sec in self.comparable_sections}
        self.section_by_title_text = {sec.title_text: sec
                                      for sec in self.comparable_sections
                                      if sec.title_text}
        self.section_by_number = {sec.section_number: sec
                                  for sec in self.comparable_sections
                                  if sec.section_number}
        self.page_map = compute_section_page_map(self.comparable_sections)
        return sections

    def find_corresponding_section(self, section):
        """Find our own section corresponding to the supplied section from another PDF."""
        own_section = self.section_by_title.get(section.title)
        if own_section:
            # Easy - full title matches
            return own_section

        own_section = self.section_by_title_text.get(section.title_text)
        if own_section:
            # Not as easy, we had a section renumber, possible issue here!
            return own_section

        own_section = self.section_by_number.get(section.section_number)
        if own_section:
            # Only the section number matched - WARNING! might be bad match!
            return own_section

        # Total failure
        return None


@attr.s
class MatchingSection:
    title = attr.ib()
    orig_range = attr.ib()
    new_range = attr.ib()
    changes = attr.ib(default=None)

    def __str__(self):
        return '{} ({}:{}-{}, {}:{}-{})'.format(
            self.title,

            self.orig_range['fn'],
            self.orig_range['page_start'],
            self.orig_range['page_end'],

            self.new_range['fn'],
            self.new_range['page_start'],
            self.new_range['page_end'],
        )


def get_section_range_pairs(orig_section, new_pdf):
    """Return MatchingSection for a section."""
    other_section = new_pdf.find_corresponding_section(orig_section)
    if not other_section:
        print(f"Skipping section {orig_section.title} - no match in the other doc!")
        return None
    return MatchingSection(
        title=orig_section.title,
        orig_range=orig_section.pdf_diff_options,
        new_range=other_section.pdf_diff_options)


def get_section_page_pairs(orig_section, new_pdf):
    """Return (orig_page_num, new_page_num) pairs for each page in section."""
    other_section = new_pdf.find_corresponding_section(orig_section)
    if not other_section:
        print(f"Skipping section {orig_section.title} - no match in the other doc!")
        return []
    return zip_longest(orig_section.page_numbers, other_section.page_numbers)


def get_page_pairs_by_section(orig_pdf, new_pdf):
    """Return an iterable of lists of (orig_page_num, new_page_num) pairs.

    One such list of pairs is returned for each section in orig_pdf."""
    return (list(get_section_page_pairs(sec, new_pdf))
            for sec in orig_pdf.comparable_sections)


def get_all_page_pairs(orig_pdf, new_pdf):
    """Get a single list of all page pairs.

    This accommodates inserted pages between sections."""
    # Flatten into a single list of pairs
    raw_pairs = list(chain.from_iterable(
        get_page_pairs_by_section(orig_pdf, new_pdf)))
    # For any "full pair" (where both parts are non-None),
    # we can skip any half-diffs involving either page
    # in the next pass. We compute that set here.

    # For example, if we have the pairs:
    # (1, 1), (2, None), (2, 1), (3, 2)
    # we can drop the (2, None) pair because the original page 2
    # is already being compared against the new page 1 -- see the (2, 1) --
    # so there's no sense in saying it only exists in the original.
    unique_full_pairs = set(((orig_page, new_page)
                             for orig_page, new_page in raw_pairs
                             if orig_page is not None
                             and new_page is not None))
    skip_orig_only = set(((orig_page, None)
                          for orig_page, _ in unique_full_pairs))
    skip_new_only = set(((None, new_page)
                         for _, new_page in unique_full_pairs))
    skip_half_pairs = skip_orig_only.union(skip_new_only)

    # Main filter pass: deduplicate and filter out excluded half-diffs
    pairs = []
    included_pairs = set()
    for page_pair in raw_pairs:
        if page_pair in skip_half_pairs:
            print("Dropping half-pair covered by other full pair", page_pair)
            # Don't unnecessarily include a "page only in document X" item.
            continue
        if page_pair in included_pairs:
            print("Dropping duplicated pair", page_pair)
            # Don't let small sections result in big dupes.
            continue
        pairs.append(page_pair)
        included_pairs.add(page_pair)

    return pairs


class SequenceGapFinder:
    def __init__(self):
        self.prev = None

    def process_and_get_gap(self, current):
        """Return the range of expected numbers skipped between the last call
        to this method and the current one.

        Return None if no numbers skipped."""
        ret = None
        if current is not None:
            if self.prev is not None and current > self.prev + 1:
                ret = range(self.prev + 1, current)
            self.prev = current
        return ret


def zip_longest_permitting_none(a, b):
    """Do zip_longest except treat None as a zero-element iterable."""
    if a is None and b is None:
        return ()
    if a is None:
        return ((None, elt) for elt in b)
    if b is None:
        return ((elt, None) for elt in a)
    return zip_longest(a, b)


def fill_pair_gaps(pairs):
    """Add any missing pages to the list of pairs."""
    orig_pages = [orig_page for orig_page, _ in pairs
                  if orig_page is not None]
    new_pages = [new_page for _, new_page in pairs
                 if new_page is not None]

    assert orig_pages == sorted(orig_pages)
    assert new_pages == sorted(new_pages)

    fixed_pairs = []

    orig_gaps = SequenceGapFinder()
    new_gaps = SequenceGapFinder()
    for orig_page, new_page in pairs:
        orig_gap = orig_gaps.process_and_get_gap(orig_page)
        new_gap = new_gaps.process_and_get_gap(new_page)
        if orig_gap is not None or new_gap is not None:
            gap_pairs = list(zip_longest_permitting_none(orig_gap, new_gap))
            print("Found gap pairs", gap_pairs)
            fixed_pairs.extend(gap_pairs)
        fixed_pairs.append((orig_page, new_page))
    return fixed_pairs


class GranularPdfDiff:
    def __init__(self, orig_fn, new_fn):
        self.orig_pdf = PdfSpec(orig_fn)
        self.new_pdf = PdfSpec(new_fn)

    def generate_matching_sections(self):
        """Return a generator of MatchingSection.

        At most one MatchingSection is returned for each section in orig_pdf.
        """
        for sec in self.orig_pdf.comparable_sections:
            matching = get_section_range_pairs(sec, self.new_pdf)
            if not matching:
                continue
            matching.orig_range["dom"] = self.orig_pdf.dom
            matching.new_range["dom"] = self.new_pdf.dom
            changes = pdf_diff.compute_changes(
                matching.orig_range, matching.new_range, bottom_margin=93)
            if changes:
                matching.changes = changes
                yield matching
            else:
                print("No changes in", matching)

    def compute_sections(self, **kwargs):
        self.orig_pdf.compute_sections(**kwargs)
        self.new_pdf.compute_sections(**kwargs)

    def compute_page_pairs(self):
        pairs = get_all_page_pairs(self.orig_pdf, self.new_pdf)
        return fill_pair_gaps(pairs)

    def generate_page_diff(self, orig_page_num, new_page_num):
        orig_page = self.orig_pdf.pdf_for_page(orig_page_num)
        new_page = self.new_pdf.pdf_for_page(new_page_num)
        if orig_page and new_page:
            changes = pdf_diff.compute_changes(
                orig_page, new_page)
            pprint(changes)
        # TODO

    def generate_diff_from_pairs(self, pairs):
        out_docs = []
        for orig_page_num, new_page_num in pairs:
            page_diff = self.generate_page_diff(orig_page_num, new_page_num)
            if page_diff:
                out_docs.append(page_diff)
        # TODO


if __name__ == "__main__":
    SPECDIR = Path(__file__).resolve().parent.parent
    assert SPECDIR.name == "specification"
    ORIG = SPECDIR / 'compare-base' / 'openxr.pdf'
    NEW = SPECDIR / 'generated' / 'out' / '1.1' / 'openxr.pdf'
    DIFFDIR = SPECDIR / 'diffs'
    DIFFDIR.mkdir(exist_ok=True)

    def is_separate_diff_section(bookmark):
        # All chapters, except the extension chapter
        if bookmark.level == 1 and "List of Extensions" not in bookmark.title:
            return True
        # All the individual sub-sections in the extension chapter
        # (one for each extension)
        if bookmark.level == 2 and "XR_KHR" in bookmark.title:
            return True
        return False

    diff = GranularPdfDiff(ORIG, NEW)
    diff.compute_sections(bookmark_predicate=is_separate_diff_section)
    for i, matching in enumerate(diff.generate_matching_sections(), 1):
        img = pdf_diff.render_changes(matching.changes,
                                      ('strike', 'underline'),
                                      900)
        fn = f"Diff part {i:02d} - {matching.title}.diff.png"
        full_path = DIFFDIR / fn

        print('Writing', full_path.relative_to(SPECDIR))
        with open(str(full_path), 'wb') as fp:
            img.save(fp, 'PNG')
