# Copyright 2023-2025 The Khronos Group Inc.
# SPDX-License-Identifier: Apache-2.0

# Adjust extension TOC entries to not break between section number and extension name.

require 'asciidoctor/extensions' unless RUBY_ENGINE == 'opal'

include ::Asciidoctor

class NonbreakingExtTitlesPostprocessor < Asciidoctor::Extensions::Postprocessor
  def process document, output
    if document.basebackend? 'html'
      return output.gsub(/(?<before>[>][0-9]+\.[0-9]+\.) (?<after>XR_[^<]+)/, '\k<before>&nbsp;\k<after>')
    end
    output
  end
end

Asciidoctor::Extensions.register do
  postprocessor NonbreakingExtTitlesPostprocessor
end
