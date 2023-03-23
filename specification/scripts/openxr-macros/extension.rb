# Copyright (c) 2016-2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

require 'asciidoctor/extensions' unless RUBY_ENGINE == 'opal'

include ::Asciidoctor

class OpenXRInlineMacroBase < Extensions::InlineMacroProcessor
    use_dsl
    using_format :short
end

class NormativeInlineMacroBase < OpenXRInlineMacroBase
    def text
        'normative'
    end

    def process parent, target, attributes
        create_inline parent, :quoted, '<strong class="purple">' + text + '</strong>'
    end
end

class LinkInlineMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes
      if parent.document.attributes['cross-file-links']
        return Inline.new(parent, :anchor, target, :type => :link, :target => (target + '.html'))
      else
        return Inline.new(parent, :anchor, target, :type => :xref, :target => ('#' + target), :attributes => {'fragment' => target, 'refid' => target})
      end
    end
end

class CodeInlineMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes

        create_inline parent, :quoted, '<code>' + target + '</code>'
    end
end

class StrongInlineMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes

        create_inline parent, :quoted, '<code>' + target + '</code>'
    end
end

class ParamInlineMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes

        create_inline parent, :quoted, '<code>' + target + '</code>'
    end
end

class CanInlineMacro < NormativeInlineMacroBase
    named :can
    match /can:(\w*)/

    def text
        'can'
    end
end

class CannotInlineMacro < NormativeInlineMacroBase
    named :cannot
    match /cannot:(\w*)/

    def text
        'cannot'
    end
end

class MayInlineMacro < NormativeInlineMacroBase
    named :may
    match /may:(\w*)/

    def text
        'may'
    end
end

class MustInlineMacro < NormativeInlineMacroBase
    named :must
    match /must:(\w*)/

    def text
        'must'
    end
end

class OptionalInlineMacro < NormativeInlineMacroBase
    named :optional
    match /optional:(\w*)/

    def text
        'optional'
    end
end

class RequiredInlineMacro < NormativeInlineMacroBase
    named :required
    match /required:(\w*)/

    def text
        'required'
    end
end

class ShouldInlineMacro < NormativeInlineMacroBase
    named :should
    match /should:(\w*)/

    def text
        'should'
    end
end

class ReflinkInlineMacro < LinkInlineMacroBase
    named :reflink
    match /reflink:(\w+)/
end

class FlinkInlineMacro < LinkInlineMacroBase
    named :flink
    match /flink:(\w+)/
end

class FnameInlineMacro < StrongInlineMacroBase
    named :fname
    match /fname:(\w+)/
end

class FtextInlineMacro < StrongInlineMacroBase
    named :ftext
    match /ftext:([\w\*]+)/
end

class SnameInlineMacro < CodeInlineMacroBase
    named :sname
    match /sname:(\w+)/
end

class SlinkInlineMacro < LinkInlineMacroBase
    named :slink
    match /slink:(\w+)/
end

class StextInlineMacro < CodeInlineMacroBase
    named :stext
    match /stext:([\w\*]+)/
end

class EnameInlineMacro < CodeInlineMacroBase
    named :ename
    match /ename:(\w+)/
end

class ElinkInlineMacro < LinkInlineMacroBase
    named :elink
    match /elink:(\w+)/
end

class EtextInlineMacro < CodeInlineMacroBase
    named :etext
    match /etext:([\w\*]+)/
end

class PnameInlineMacro < ParamInlineMacroBase
    named :pname
    match /pname:((\w[\w.]*)*\w+)/
end

class PtextInlineMacro < ParamInlineMacroBase
    named :ptext
    match /ptext:((\w[\w.]*)*\w+)/
end

class DnameInlineMacro < CodeInlineMacroBase
    named :dname
    match /dname:(\w+)/
end

class DlinkInlineMacro < LinkInlineMacroBase
    named :dlink
    match /dlink:(\w+)/
end

class TnameInlineMacro < CodeInlineMacroBase
    named :tname
    match /tname:(\w+)/
end

class TlinkInlineMacro < LinkInlineMacroBase
    named :tlink
    match /tlink:(\w+)/
end

class BasetypeInlineMacro < LinkInlineMacroBase
    named :basetype
    match /basetype:(\w+)/
    def process parent, target, attributes
      if parent.document.attributes['cross-file-links']
        return Inline.new(parent, :anchor, target, :type => :link, :target => (target + '.html'))
      else
        return Inline.new(parent, :anchor, '<code>' + target + '</code>', :type => :xref, :target => ('#' + target), :attributes => {'fragment' => target, 'refid' => target})
      end
    end
end

class CodeInlineMacro < StrongInlineMacroBase
    named :code
    match /code:([*\w]+)/
end

class SemanticSubpathMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes
        create_inline parent, :quoted, '<em>&#8230;' + target + '</em>'
    end
end

class SemanticSubpathInlineMacro < SemanticSubpathMacroBase
    named :subpathname
    match /subpathname:([\w\/\-\.]+)/
end

class SemanticPathMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes
        create_inline parent, :quoted, '<em>' + target + '</em>'
    end
end

class SemanticPathInlineMacro < SemanticPathMacroBase
    named :pathname
    match /pathname:([\w\/\-\.]+)/
end

class ActionRelatedNameMacroBase < OpenXRInlineMacroBase
    def process parent, target, attributes
        create_inline parent, :quoted, '<code>' + target + '</code>'
    end
end

class ActionNameInlineMacro < ActionRelatedNameMacroBase
    named :actionname
    match /actionname:([\w\-]+)/
end

# Could effectively repeat if we need an actionsetname:

