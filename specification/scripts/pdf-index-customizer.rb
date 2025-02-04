# Copyright (c) 2020-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# This file customizes the way that asciidoctor-pdf makes the index,
# so it's not all uselessly sorted under "X" since all the terms start with "XR"

# see https://github.com/asciidoctor/asciidoctor-pdf#how-index-terms-are-grouped-and-sorted

require "asciidoctor-pdf"

module Asciidoctor::Pdf
  module OpenXR
    # Normalizes an OpenXR entity name to just the part that matters for ordering.
    def strip_api_prefix(name)
      name.sub /^[xX][rR](_?)/, ""
    end
  end

  # Override how the "Category" (first letter heading) is computed
  # Docs and source for original version:
  # https://www.rubydoc.info/github/asciidoctor/asciidoctor-pdf/Asciidoctor/Pdf/IndexCatalog#store_primary_term-instance_method
  module IndexCatalogExtension
    include Sanitizer
    include OpenXR

    def store_primary_term(name, dest = nil)
      store_dest dest if dest
      # After stripping the prefix (if any) do a multibyte-uppercase
      # and grab the first character as the category
      category = strip_api_prefix(name).upcase.chr
      (init_category category).store_term name, dest
    end
  end

  # Override how index terms are sorted: they ignore their prefix.
  # Docs and source for original version:
  # https://www.rubydoc.info/github/asciidoctor/asciidoctor-pdf/Asciidoctor/Pdf/IndexTermGroup#%3C=%3E-instance_method
  module IndexTermGroupExtension
    include OpenXR

    def <=>(other)
      this = strip_api_prefix(@name)
      that = strip_api_prefix(other.name)
      (val = this.casecmp that) == 0 ? this <=> that : val
    end
  end

  if const_defined?("IndexCatalog")
    IndexCatalog.prepend(IndexCatalogExtension)
  else
    println("Note: Could not apply customization to PDF index category generation")
  end

  if const_defined?("IndexTermGroup")
    IndexCatalog.prepend(IndexTermGroupExtension)
  else
    println("Note: Could not apply customization to PDF index sorting")
  end
end
