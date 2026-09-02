//------------------------------------------------------------------------------
// FormatDocument.cpp
// Formatter layout IR and line solving.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/FormatDocument.h"

#include "format/FormatConstants.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <optional>
#include <tuple>

namespace format {
namespace {

struct FlatAtom {
    enum class Kind {
        Text,
        Verbatim,
        AbsoluteText,
        SoftLine,
        HardLine,
        AnchorStart,
        AnchorEnd,
        Alignment
    };
    Kind kind = Kind::Text;
    std::string_view text;
    BreakId breakId = 0;
    GroupId groupId = 0;
    int priority = 0;
    int hardLineCount = 1;
    int indent = 0;
    AnchorId anchorId = 0;
    AlignmentId alignmentId = 0;
    uint32_t alignmentColumn = 0;
    slang::syntax::SyntaxKind syntaxKind = slang::syntax::SyntaxKind::Unknown;
    MemberId memberId = 0;
    int anchorIndent = 0;
    BreakId conditionalIndentBreak = 0;
    int conditionalIndent = 0;
    size_t anchorFitWidth = 0;
    size_t minimumPadding = 0;
    AlignmentGroupId alignmentGroup = 0;
};

struct FlattenContext {
    int indent = 0;
    int anchorIndent = 0;
    size_t anchorDepth = 0;
    GroupId group = 0;
    MemberId member = 0;
    BreakId conditionalIndentBreak = 0;
    int conditionalIndent = 0;
};

void flatten(const FormatDocument& document, DocId id, FlattenContext context,
             std::vector<FlatAtom>& result) {
    const auto& node = document.nodes.at(id);
    switch (node.kind) {
        case DocKind::Empty:
            break;
        case DocKind::Text:
            result.push_back({FlatAtom::Kind::Text, node.text, 0, 0, 0, 1, context.indent, 0, 0, 0,
                              node.syntaxKind, context.member});
            break;
        case DocKind::Verbatim:
            result.push_back({FlatAtom::Kind::Verbatim, node.text, 0, 0, 0, 1, context.indent, 0, 0,
                              0, node.syntaxKind, context.member});
            break;
        case DocKind::AbsoluteText:
            result.push_back({FlatAtom::Kind::AbsoluteText, node.text, 0, 0, 0, 1, context.indent,
                              0, 0, 0, node.syntaxKind, context.member});
            break;
        case DocKind::SoftLine:
            result.push_back({FlatAtom::Kind::SoftLine, node.text, node.id,
                              node.group ? node.group : context.group, node.value, 1,
                              context.indent, 0, 0, 0, node.syntaxKind,
                              node.global ? 0 : context.member});
            result.back().anchorIndent = context.anchorIndent;
            result.back().conditionalIndentBreak = context.conditionalIndentBreak;
            result.back().conditionalIndent = context.conditionalIndent;
            break;
        case DocKind::HardLine:
            result.push_back({FlatAtom::Kind::HardLine,
                              {},
                              0,
                              context.group,
                              0,
                              node.value,
                              context.indent,
                              0,
                              0,
                              0,
                              node.syntaxKind,
                              context.member});
            result.back().anchorIndent = node.id ? context.anchorIndent : -1;
            result.back().conditionalIndentBreak = context.conditionalIndentBreak;
            result.back().conditionalIndent = context.conditionalIndent;
            break;
        case DocKind::Concat:
            for (auto child : node.children)
                flatten(document, child, context, result);
            break;
        case DocKind::Indent:
            if (context.anchorDepth)
                context.anchorIndent += node.value;
            else
                context.indent += node.value;
            flatten(document, node.children.front(), context, result);
            break;
        case DocKind::IndentIfBreak:
            context.conditionalIndentBreak = node.id;
            context.conditionalIndent += node.value;
            flatten(document, node.children.front(), context, result);
            break;
        case DocKind::RelativeAnchor:
            result.push_back({FlatAtom::Kind::AnchorStart,
                              {},
                              0,
                              0,
                              node.value,
                              1,
                              context.indent,
                              node.id,
                              0,
                              0,
                              node.syntaxKind,
                              context.member});
            result.back().anchorFitWidth = node.width;
            context.anchorDepth++;
            context.anchorIndent = 0;
            flatten(document, node.children.front(), context, result);
            result.push_back({FlatAtom::Kind::AnchorEnd,
                              {},
                              0,
                              0,
                              0,
                              1,
                              context.indent,
                              node.id,
                              0,
                              0,
                              node.syntaxKind,
                              context.member});
            break;
        case DocKind::ConsistentGroup:
            context.group = node.id;
            flatten(document, node.children.front(), context, result);
            break;
        case DocKind::AlignmentAnchor:
            result.push_back({FlatAtom::Kind::Alignment,
                              {},
                              0,
                              0,
                              0,
                              1,
                              context.indent,
                              0,
                              node.id,
                              static_cast<uint32_t>(node.value),
                              node.syntaxKind,
                              context.member});
            result.back().minimumPadding = node.width;
            result.back().alignmentGroup = node.alignmentGroup;
            break;
        case DocKind::Member:
            context.member = node.id;
            flatten(document, node.children.front(), context, result);
            break;
    }
}

struct Cost {
    size_t worstOverflow = 0;
    size_t totalOverflow = 0;
    size_t overflowingLines = 0;
    size_t breaks = 0;
    size_t raggedness = 0;

    auto tie() const {
        return std::tie(worstOverflow, totalOverflow, overflowingLines, breaks, raggedness);
    }
    bool operator<(const Cost& rhs) const { return tie() < rhs.tie(); }
};

struct RenderOptions {
    std::unordered_set<BreakId> breaks;
    std::unordered_map<AlignmentId, size_t> padding;
    std::unordered_set<AlignmentId> alignedContinuations;
};

struct RenderResult {
    RenderedDocument rendered;
    Cost cost;
    std::unordered_map<MemberId, Cost> memberCosts;
};

void finishLine(Cost& cost, size_t column, uint32_t limit) {
    if (limit && column > limit) {
        size_t overflow = column - limit;
        cost.worstOverflow = std::max(cost.worstOverflow, overflow);
        cost.totalOverflow += overflow;
        cost.overflowingLines++;
    }
    else if (limit) {
        size_t remainder = limit - column;
        cost.raggedness += remainder * remainder;
    }
}

RenderResult renderAtoms(const std::vector<FlatAtom>& atoms, const Config& config,
                         const RenderOptions& options, bool collectText) {
    RenderResult result;
    auto& rendered = result.rendered;
    size_t column = 0;
    size_t line = 0;
    size_t pendingIndent = 0;
    bool atLineStart = true;
    std::unordered_set<MemberId> lineMembers;
    MemberId registeredLineMember = 0;
    MemberId activeMember = 0;
    std::unordered_map<AnchorId, size_t> anchors;
    std::vector<std::pair<AnchorId, std::optional<size_t>>> anchorStack;
    std::unordered_set<GroupId> forcedGroups;
    struct Delimiter {
        char close;
        size_t column;
        size_t contentIndent;
        bool aligned;
    };
    std::vector<Delimiter> delimiters;
    bool pendingAlignedDelimiter = false;
    std::unordered_map<MemberId, size_t> memberContinuationIndents;
    std::unordered_set<GroupId> brokenGroups;
    for (const auto& atom : atoms) {
        if (atom.kind == FlatAtom::Kind::HardLine && atom.groupId)
            forcedGroups.insert(atom.groupId);
        else if (atom.kind == FlatAtom::Kind::SoftLine && atom.groupId &&
                 options.breaks.contains(atom.breakId)) {
            brokenGroups.insert(atom.groupId);
        }
    }

    auto applyIndent = [&]() {
        if (!atLineStart)
            return;
        if (collectText && pendingIndent)
            rendered.text.append(pendingIndent, ' ');
        column = pendingIndent;
        pendingIndent = 0;
        atLineStart = false;
    };

    auto append = [&](std::string_view text) {
        if (text.empty())
            return;
        if (text.front() != '\n')
            applyIndent();
        for (char c : text) {
            if (c == '\n') {
                finishLine(result.cost, column, config.columnLimit.get());
                for (auto member : lineMembers)
                    finishLine(result.memberCosts[member], column, config.columnLimit.get());
                if (collectText)
                    rendered.text.push_back('\n');
                line++;
                column = 0;
                pendingIndent = 0;
                atLineStart = true;
                lineMembers.clear();
                registeredLineMember = 0;
            }
            else {
                if (atLineStart)
                    applyIndent();
                if (activeMember && activeMember != registeredLineMember) {
                    lineMembers.insert(activeMember);
                    registeredLineMember = activeMember;
                }
                if (collectText)
                    rendered.text.push_back(c);
                column++;
            }
        }
    };

    auto newline = [&](int count, size_t indent) {
        for (int i = 0; i < count; i++) {
            if (!atLineStart || i > 0) {
                finishLine(result.cost, column, config.columnLimit.get());
                for (auto member : lineMembers)
                    finishLine(result.memberCosts[member], column, config.columnLimit.get());
                if (collectText)
                    rendered.text.push_back('\n');
                line++;
                column = 0;
                atLineStart = true;
                lineMembers.clear();
                registeredLineMember = 0;
            }
        }
        pendingIndent = indent;
    };

    for (const auto& atom : atoms) {
        activeMember = atom.memberId;
        auto breakIndent = [&]() {
            size_t indent = std::max(atom.indent, 0);
            if (atom.anchorIndent >= 0 && !anchorStack.empty()) {
                auto found = anchors.find(anchorStack.back().first);
                if (found != anchors.end())
                    indent = std::max(indent, found->second + static_cast<size_t>(
                                                                  std::max(atom.anchorIndent, 0)));
                else
                    indent += static_cast<size_t>(std::max(atom.anchorIndent, 0));
            }
            if (atom.conditionalIndentBreak &&
                options.breaks.contains(atom.conditionalIndentBreak)) {
                indent += static_cast<size_t>(std::max(atom.conditionalIndent, 0));
            }
            if (!delimiters.empty() && delimiters.back().aligned)
                indent = std::max(indent, delimiters.back().contentIndent);
            if (auto it = memberContinuationIndents.find(atom.memberId);
                it != memberContinuationIndents.end()) {
                indent = std::max(indent, it->second);
            }
            return indent;
        };
        switch (atom.kind) {
            case FlatAtom::Kind::Text: {
                bool closingDelimiter = atom.text.size() == 1 && !delimiters.empty() &&
                                        atom.text.front() == delimiters.back().close;
                if (closingDelimiter && atLineStart && delimiters.back().aligned)
                    pendingIndent = delimiters.back().column;
                applyIndent();
                size_t tokenColumn = column;
                append(atom.text);
                if (closingDelimiter) {
                    delimiters.pop_back();
                }
                else if (atom.text == "(" || atom.text == "{" || atom.text == "[") {
                    bool aligned = pendingAlignedDelimiter ||
                                   (!delimiters.empty() && delimiters.back().aligned);
                    size_t extra = atom.text == "(" ? 5 : 4;
                    delimiters.push_back({atom.text == "("   ? ')'
                                          : atom.text == "{" ? '}'
                                                             : ']',
                                          tokenColumn, tokenColumn + extra, aligned});
                }
                pendingAlignedDelimiter = false;
                break;
            }
            case FlatAtom::Kind::Verbatim:
                append(atom.text);
                break;
            case FlatAtom::Kind::AbsoluteText:
                pendingIndent = 0;
                column = 0;
                atLineStart = false;
                append(atom.text);
                break;
            case FlatAtom::Kind::SoftLine: {
                bool split = options.breaks.contains(atom.breakId);
                if (!split && atom.groupId)
                    split = forcedGroups.contains(atom.groupId) ||
                            brokenGroups.contains(atom.groupId);
                if (split) {
                    newline(1, breakIndent());
                    rendered.breaks.insert(atom.breakId);
                    rendered.renderedBreaks.push_back(
                        {atom.breakId, collectText ? rendered.text.size() : 0});
                    result.cost.breaks++;
                    if (atom.memberId)
                        result.memberCosts[atom.memberId].breaks++;
                }
                else {
                    append(atom.text);
                }
                break;
            }
            case FlatAtom::Kind::HardLine:
                newline(atom.hardLineCount, breakIndent());
                break;
            case FlatAtom::Kind::AnchorStart: {
                applyIndent();
                auto it = anchors.find(atom.anchorId);
                anchorStack.emplace_back(atom.anchorId, it == anchors.end()
                                                            ? std::optional<size_t>{}
                                                            : std::optional<size_t>{it->second});
                auto target = static_cast<int64_t>(column) + atom.priority;
                bool fits = target >= 0 && (!atom.anchorFitWidth || !config.columnLimit.get() ||
                                            static_cast<size_t>(target) + atom.anchorFitWidth <=
                                                config.columnLimit.get());
                if (fits)
                    anchors[atom.anchorId] = static_cast<size_t>(target);
                break;
            }
            case FlatAtom::Kind::AnchorEnd: {
                if (!anchorStack.empty()) {
                    auto [id, old] = anchorStack.back();
                    anchorStack.pop_back();
                    if (old)
                        anchors[id] = *old;
                    else
                        anchors.erase(id);
                }
                break;
            }
            case FlatAtom::Kind::Alignment: {
                applyIndent();
                auto it = options.padding.find(atom.alignmentId);
                if (it != options.padding.end() && it->second) {
                    if (collectText)
                        rendered.text.append(it->second, ' ');
                    column += it->second;
                }
                if (options.alignedContinuations.contains(atom.alignmentId)) {
                    if (atom.syntaxKind == slang::syntax::SyntaxKind::AssignmentPatternItem &&
                        atom.memberId) {
                        memberContinuationIndents[atom.memberId] = column + 2;
                    }
                    else {
                        pendingAlignedDelimiter = true;
                    }
                }
                rendered.alignmentAnchors.push_back(
                    {atom.alignmentId, atom.alignmentColumn, atom.syntaxKind, atom.alignmentGroup,
                     atom.memberId, line, column, collectText ? rendered.text.size() : 0,
                     atom.minimumPadding});
                break;
            }
        }
    }
    finishLine(result.cost, column, config.columnLimit.get());
    for (auto member : lineMembers)
        finishLine(result.memberCosts[member], column, config.columnLimit.get());
    return result;
}

struct Action {
    MemberId member = 0;
    int priority = 0;
    std::vector<BreakId> breaks;
};

std::vector<Action> collectActions(const std::vector<FlatAtom>& atoms,
                                   const std::unordered_set<BreakId>& fixed) {
    std::map<GroupId, Action> groups;
    std::vector<Action> actions;
    for (const auto& atom : atoms) {
        if (atom.kind != FlatAtom::Kind::SoftLine || fixed.contains(atom.breakId))
            continue;
        if (!atom.groupId) {
            actions.push_back({atom.memberId, atom.priority, {atom.breakId}});
            continue;
        }
        auto& action = groups[atom.groupId];
        if (action.breaks.empty()) {
            action.member = atom.memberId;
            action.priority = atom.priority;
        }
        else {
            action.priority = std::min(action.priority, atom.priority);
        }
        action.breaks.push_back(atom.breakId);
    }
    for (auto& [id, action] : groups)
        actions.push_back(std::move(action));
    std::ranges::sort(actions, [](const Action& left, const Action& right) {
        if (auto ordering = std::tie(left.member, left.priority) <=
                            std::tie(right.member, right.priority);
            std::tie(left.member, left.priority) != std::tie(right.member, right.priority)) {
            return ordering;
        }
        return left.breaks.front() > right.breaks.front();
    });
    return actions;
}

Cost memberCost(const RenderResult& result, MemberId member) {
    if (!member)
        return result.cost;
    if (auto it = result.memberCosts.find(member); it != result.memberCosts.end())
        return it->second;
    return {};
}

RenderOptions solve(const std::vector<FlatAtom>& atoms, const Config& config, RenderOptions base) {
    if (!config.columnLimit.get())
        return base;

    auto actions = collectActions(atoms, base.breaks);
    if (actions.empty())
        return base;

    size_t memberBegin = 0;
    while (memberBegin < actions.size()) {
        MemberId member = actions[memberBegin].member;
        size_t memberEnd = memberBegin + 1;
        while (memberEnd < actions.size() && actions[memberEnd].member == member)
            memberEnd++;

        auto baseline = memberCost(renderAtoms(atoms, config, base, false), member);
        if (baseline.worstOverflow == 0) {
            memberBegin = memberEnd;
            continue;
        }

        struct State {
            RenderOptions options;
            Cost cost;
        };
        std::vector<State> states{{base, baseline}};
        size_t tierBegin = memberBegin;
        while (tierBegin < memberEnd) {
            size_t tierEnd = tierBegin + 1;
            while (tierEnd < memberEnd &&
                   actions[tierEnd].priority == actions[tierBegin].priority) {
                tierEnd++;
            }

            for (size_t actionIndex = tierBegin; actionIndex < tierEnd; actionIndex++) {
                size_t oldSize = states.size();
                for (size_t stateIndex = 0; stateIndex < oldSize; stateIndex++) {
                    if (states[stateIndex].cost.worstOverflow == 0)
                        continue;
                    auto candidate = states[stateIndex].options;
                    candidate.breaks.insert(actions[actionIndex].breaks.begin(),
                                            actions[actionIndex].breaks.end());
                    auto rendered = renderAtoms(atoms, config, candidate, false);
                    states.push_back({candidate, memberCost(rendered, member)});
                }
                std::ranges::sort(states, [](const State& left, const State& right) {
                    return left.cost < right.cost;
                });
                constexpr size_t beamWidth = 512;
                if (states.size() > beamWidth)
                    states.resize(beamWidth);
            }
            if (states.front().cost.worstOverflow == 0)
                break;
            tierBegin = tierEnd;
        }

        base = std::move(states.front().options);
        memberBegin = memberEnd;
    }
    return base;
}

struct ComputedAlignment {
    std::unordered_map<AlignmentId, size_t> padding;
    std::unordered_set<AlignmentId> continuations;
};

ComputedAlignment computeAlignment(const RenderedDocument& layout, const Config& config) {
    struct GroupKey {
        slang::syntax::SyntaxKind kind;
        uint32_t column;
        AlignmentGroupId group;
        bool operator<(const GroupKey& rhs) const {
            return std::tie(kind, column, group) < std::tie(rhs.kind, rhs.column, rhs.group);
        }
    };
    std::map<GroupKey, std::vector<const RenderedAlignmentAnchor*>> groups;
    for (const auto& anchor : layout.alignmentAnchors)
        groups[{anchor.rowKind, anchor.columnIndex, anchor.group}].push_back(&anchor);

    std::vector<std::string_view> lines;
    size_t lineStart = 0;
    while (lineStart <= layout.text.size()) {
        size_t lineEnd = layout.text.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = layout.text.size();
        lines.emplace_back(layout.text.data() + lineStart, lineEnd - lineStart);
        if (lineEnd == layout.text.size())
            break;
        lineStart = lineEnd + 1;
    }
    auto separatorCount = [&](size_t previousLine, size_t nextLine, int limit,
                              bool ignoreContent = false) {
        int separators = 0;
        int commentLines = 0;
        auto finishComment = [&] {
            if (commentLines > 0)
                separators += commentLines - 1;
            commentLines = 0;
        };
        for (size_t line = previousLine + 1; line < nextLine && line < lines.size(); line++) {
            auto text = lines[line];
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            if (text.empty()) {
                finishComment();
                separators++;
            }
            else if (text.starts_with("//") || text.starts_with("/*") || text.starts_with('*') ||
                     text.starts_with("*/")) {
                commentLines++;
            }
            else {
                if (!ignoreContent)
                    return limit;
            }
        }
        finishComment();
        return separators;
    };
    auto leadingIndent = [&](size_t line) {
        size_t indent = 0;
        while (line < lines.size() && indent < lines[line].size() &&
               (lines[line][indent] == ' ' || lines[line][indent] == '\t')) {
            indent++;
        }
        return indent;
    };
    auto isTerminalAssignmentAnchor = [&](const RenderedAlignmentAnchor& anchor) {
        if (anchor.columnIndex < 2 || anchor.outputOffset >= layout.text.size())
            return false;
        size_t end = layout.text.find('\n', anchor.outputOffset);
        if (end == std::string::npos)
            end = layout.text.size();
        auto suffix =
            std::string_view(layout.text).substr(anchor.outputOffset, end - anchor.outputOffset);
        while (!suffix.empty() && (suffix.back() == ' ' || suffix.back() == '\t'))
            suffix.remove_suffix(1);
        return suffix == "=" || suffix == "<=" ||
               (suffix.find('=') != std::string_view::npos && suffix.ends_with('{'));
    };
    auto lineSuffix = [&](const RenderedAlignmentAnchor& anchor) {
        size_t end = layout.text.find('\n', anchor.outputOffset);
        if (end == std::string::npos)
            end = layout.text.size();
        return std::string_view(layout.text).substr(anchor.outputOffset, end - anchor.outputOffset);
    };

    ComputedAlignment result;
    std::map<size_t, size_t> rowShifts;
    for (auto& [key, anchors] : groups) {
        if (key.column == 100) {
            std::unordered_set<MemberId> members;
            for (const auto* anchor : anchors)
                members.insert(anchor->member);
            if (members.size() < 2)
                continue;
        }
        std::ranges::sort(anchors, {}, [](const auto* anchor) { return anchor->line; });
        std::vector<const RenderedAlignmentAnchor*> rows;
        for (const auto* anchor : anchors) {
            auto line = lines[anchor->line];
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.remove_prefix(1);
            if (line.starts_with('`'))
                continue;
            if (!rows.empty() && rows.back()->line == anchor->line)
                continue;
            rows.push_back(anchor);
        }
        size_t groupBegin = 0;
        while (groupBegin < rows.size()) {
            size_t groupEnd = groupBegin + 1;
            int threshold = constants::isBodyAlignKind(key.kind)
                                ? config.alignment.get().linesBetweenGroups.get()
                                : constants::defaultGroupSeparatorLines;
            threshold = std::max(threshold, 1);
            if (isTerminalAssignmentAnchor(*rows[groupBegin])) {
                groupBegin = groupEnd;
                continue;
            }
            while (groupEnd < rows.size()) {
                if (isTerminalAssignmentAnchor(*rows[groupEnd])) {
                    groupEnd++;
                    break;
                }
                auto nextLine = lines[rows[groupEnd]->line];
                while (!nextLine.empty() && (nextLine.front() == ' ' || nextLine.front() == '\t')) {
                    nextLine.remove_prefix(1);
                }
                bool multilineRow = nextLine.starts_with('}') ||
                                    key.kind == slang::syntax::SyntaxKind::StandardCaseItem ||
                                    key.kind == slang::syntax::SyntaxKind::NamedPortConnection ||
                                    key.kind == slang::syntax::SyntaxKind::NamedParamAssignment;
                if ((key.column == 100 ||
                     key.kind == slang::syntax::SyntaxKind::AssignmentPatternItem) &&
                    leadingIndent(rows[groupEnd - 1]->line) !=
                        leadingIndent(rows[groupEnd]->line)) {
                    break;
                }
                bool ignoreContent = multilineRow ||
                                     key.kind == slang::syntax::SyntaxKind::AssignmentPatternItem;
                if (separatorCount(rows[groupEnd - 1]->line, rows[groupEnd]->line, threshold,
                                   ignoreContent) >= threshold) {
                    break;
                }
                groupEnd++;
            }
            bool enumValueColumn = key.kind == slang::syntax::SyntaxKind::Declarator &&
                                   key.column == 2;
            bool parameterValueColumn = key.kind ==
                                            slang::syntax::SyntaxKind::ParameterDeclaration &&
                                        key.column == 2;
            auto rangeOverflows = [&](size_t begin, size_t end) {
                if (!config.columnLimit.get())
                    return false;
                size_t maxColumn = 0;
                for (size_t i = begin; i < end; i++) {
                    maxColumn = std::max(maxColumn, rows[i]->column + rowShifts[rows[i]->line]);
                }
                for (size_t i = begin; i < end; i++) {
                    auto line = rows[i]->line;
                    if (isTerminalAssignmentAnchor(*rows[i]) ||
                        ((enumValueColumn || parameterValueColumn) &&
                         lineSuffix(*rows[i]).find('=') == std::string_view::npos)) {
                        continue;
                    }
                    size_t currentWidth = lines[line].size() + rowShifts[line];
                    size_t padding = maxColumn - rows[i]->column - rowShifts[line] +
                                     rows[i]->minimumPadding;
                    if (currentWidth <= config.columnLimit.get() &&
                        currentWidth + padding > config.columnLimit.get()) {
                        return true;
                    }
                }
                return false;
            };
            if (rangeOverflows(groupBegin, groupEnd)) {
                for (size_t boundary = groupBegin + 1; boundary < groupEnd; boundary++) {
                    if (separatorCount(rows[boundary - 1]->line, rows[boundary]->line, threshold) >
                        0) {
                        groupEnd = boundary;
                        break;
                    }
                }
            }
            if (key.kind == slang::syntax::SyntaxKind::DataDeclaration && key.column > 1 &&
                key.column % 2 == 1) {
                auto firstColumn = groups.find({key.kind, 1, key.group});
                if (firstColumn != groups.end()) {
                    std::unordered_set<size_t> firstColumnLines;
                    std::unordered_set<size_t> currentColumnLines;
                    size_t firstLine = rows[groupBegin]->line;
                    size_t lastLine = rows[groupEnd - 1]->line;
                    for (const auto* anchor : firstColumn->second) {
                        if (anchor->line >= firstLine && anchor->line <= lastLine)
                            firstColumnLines.insert(anchor->line);
                    }
                    for (size_t i = groupBegin; i < groupEnd; i++)
                        currentColumnLines.insert(rows[i]->line);
                    bool mixedShape = firstColumnLines != currentColumnLines;
                    for (const auto& [otherKey, otherAnchors] : groups) {
                        if (otherKey.kind != key.kind || otherKey.group != key.group ||
                            otherKey.column <= 1 || otherKey.column % 2 == 0) {
                            continue;
                        }
                        std::unordered_set<size_t> otherLines;
                        for (const auto* anchor : otherAnchors) {
                            if (anchor->line >= firstLine && anchor->line <= lastLine)
                                otherLines.insert(anchor->line);
                        }
                        if (!otherLines.empty() && otherLines != firstColumnLines) {
                            mixedShape = true;
                            break;
                        }
                    }
                    if (mixedShape) {
                        groupBegin = groupEnd;
                        continue;
                    }
                }
            }
            if (groupEnd - groupBegin == 1) {
                const auto* row = rows[groupBegin];
                auto line = lines[row->line];
                while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                    line.remove_prefix(1);
                bool hasSeparator = row->outputOffset > 0 &&
                                    (layout.text[row->outputOffset - 1] == ' ' ||
                                     layout.text[row->outputOffset - 1] == '\t');
                if (row->minimumPadding &&
                    key.kind == slang::syntax::SyntaxKind::NamedParamAssignment &&
                    key.column == 1 && line.starts_with('.') && !hasSeparator) {
                    result.padding[row->id] = row->minimumPadding;
                }
                else if (key.column == 0 && row->minimumPadding) {
                    result.padding[row->id] = row->minimumPadding;
                }
                groupBegin = groupEnd;
                continue;
            }
            if (enumValueColumn) {
                size_t initializedRows = 0;
                for (size_t i = groupBegin; i < groupEnd; i++) {
                    if (lineSuffix(*rows[i]).find('=') != std::string_view::npos)
                        initializedRows++;
                }
                if (initializedRows < 2) {
                    groupBegin = groupEnd;
                    continue;
                }
            }
            size_t maxColumn = 0;
            for (size_t i = groupBegin; i < groupEnd; i++) {
                maxColumn = std::max(maxColumn, rows[i]->column + rowShifts[rows[i]->line]);
            }
            for (size_t i = groupBegin; i < groupEnd; i++) {
                auto line = rows[i]->line;
                if (isTerminalAssignmentAnchor(*rows[i]) ||
                    ((enumValueColumn || parameterValueColumn) &&
                     lineSuffix(*rows[i]).find('=') == std::string_view::npos)) {
                    continue;
                }
                if (key.column == 0 && key.kind == slang::syntax::SyntaxKind::ImplicitAnsiPort) {
                    auto text = lines[line];
                    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
                        text.remove_prefix(1);
                    }
                    bool hasDirection = text.starts_with("input ") || text.starts_with("output ") ||
                                        text.starts_with("inout ") || text.starts_with("ref ");
                    if (!hasDirection)
                        continue;
                }
                size_t alignmentPadding = maxColumn - rows[i]->column - rowShifts[line];
                size_t padding = key.column == 0
                                     ? std::max(alignmentPadding, rows[i]->minimumPadding)
                                     : alignmentPadding + rows[i]->minimumPadding;
                bool assignmentLike =
                    key.kind == slang::syntax::SyntaxKind::ExpressionStatement ||
                    key.kind == slang::syntax::SyntaxKind::ParameterDeclarationStatement ||
                    key.kind == slang::syntax::SyntaxKind::ParameterDeclaration;
                size_t currentWidth = lines[line].size() + rowShifts[line];
                if (assignmentLike && config.columnLimit.get() &&
                    currentWidth <= config.columnLimit.get() &&
                    currentWidth + padding > config.columnLimit.get()) {
                    continue;
                }
                const auto& maxSpaces = config.alignment.get().maxSpaces.get();
                if (!maxSpaces || padding < static_cast<size_t>(*maxSpaces)) {
                    result.padding[rows[i]->id] = padding;
                    if (key.column == 1)
                        result.continuations.insert(rows[i]->id);
                    rowShifts[line] += padding;
                }
            }
            groupBegin = groupEnd;
        }
    }
    return result;
}

void normalizeRenderedText(std::string& text) {
    std::string normalized;
    normalized.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        size_t contentEnd = end;
        while (contentEnd > pos && (text[contentEnd - 1] == ' ' || text[contentEnd - 1] == '\t'))
            contentEnd--;
        normalized.append(text, pos, contentEnd - pos);
        normalized.push_back('\n');
        pos = end == text.size() ? end : end + 1;
    }
    text = std::move(normalized);
}

} // namespace

DocumentBuilder::DocumentBuilder() {
    document_.nodes.push_back({});
}

DocId DocumentBuilder::add(DocNode node) {
    document_.nodes.push_back(std::move(node));
    return static_cast<DocId>(document_.nodes.size() - 1);
}

DocId DocumentBuilder::empty() {
    return 0;
}

DocId DocumentBuilder::text(std::string_view value, slang::syntax::SyntaxKind kind) {
    if (value.empty())
        return empty();
    DocNode node;
    node.kind = DocKind::Text;
    node.text = value;
    node.syntaxKind = kind;
    return add(std::move(node));
}

DocId DocumentBuilder::verbatim(std::string_view value) {
    if (value.empty())
        return empty();
    DocNode node;
    node.kind = DocKind::Verbatim;
    node.text = value;
    return add(std::move(node));
}

DocId DocumentBuilder::absoluteText(std::string_view value) {
    if (value.empty())
        return empty();
    DocNode node;
    node.kind = DocKind::AbsoluteText;
    node.text = value;
    return add(std::move(node));
}

DocId DocumentBuilder::softLine(int priority, std::string_view flatText, GroupId group,
                                bool global) {
    DocNode node;
    node.kind = DocKind::SoftLine;
    node.text = flatText;
    node.value = priority;
    node.id = nextBreak_++;
    node.group = group;
    node.global = global;
    return add(std::move(node));
}

DocId DocumentBuilder::hardLine(int count, bool useAnchor) {
    DocNode node;
    node.kind = DocKind::HardLine;
    node.value = std::max(count, 1);
    node.id = useAnchor ? 1 : 0;
    return add(std::move(node));
}

DocId DocumentBuilder::concat(std::vector<DocId> children) {
    std::erase(children, empty());
    if (children.empty())
        return empty();
    if (children.size() == 1)
        return children.front();
    DocNode node;
    node.kind = DocKind::Concat;
    node.children = std::move(children);
    return add(std::move(node));
}

DocId DocumentBuilder::indent(int columns, DocId child) {
    if (!child)
        return child;
    DocNode node;
    node.kind = DocKind::Indent;
    node.value = columns;
    node.children.push_back(child);
    return add(std::move(node));
}

DocId DocumentBuilder::indentIfBreak(DocId line, int columns, DocId child) {
    if (!child || !line || document_.nodes.at(line).kind != DocKind::SoftLine)
        return child;
    DocNode node;
    node.kind = DocKind::IndentIfBreak;
    node.value = columns;
    node.id = document_.nodes.at(line).id;
    node.children.push_back(child);
    return add(std::move(node));
}

DocId DocumentBuilder::relativeAnchor(int offset, DocId child, size_t fitWidth) {
    if (!child)
        return child;
    DocNode node;
    node.kind = DocKind::RelativeAnchor;
    node.value = offset;
    node.id = nextAnchor_++;
    node.width = static_cast<uint32_t>(fitWidth);
    node.children.push_back(child);
    return add(std::move(node));
}

DocId DocumentBuilder::consistentGroup(DocId child) {
    if (!child)
        return child;
    DocNode node;
    node.kind = DocKind::ConsistentGroup;
    node.id = nextGroup_++;
    node.children.push_back(child);
    return add(std::move(node));
}

GroupId DocumentBuilder::createConsistentGroup() {
    return nextGroup_++;
}

AlignmentGroupId DocumentBuilder::createAlignmentGroup() {
    return nextAlignmentGroup_++;
}

DocId DocumentBuilder::alignmentAnchor(uint32_t column, slang::syntax::SyntaxKind rowKind,
                                       AlignmentGroupId group, uint32_t minimumPadding) {
    DocNode node;
    node.kind = DocKind::AlignmentAnchor;
    node.id = nextAlignment_++;
    node.value = static_cast<int>(column);
    node.width = minimumPadding;
    node.syntaxKind = rowKind;
    node.alignmentGroup = group;
    return add(std::move(node));
}

DocId DocumentBuilder::member(DocId child, slang::syntax::SyntaxKind kind) {
    if (!child)
        return child;
    DocNode node;
    node.kind = DocKind::Member;
    node.id = nextMember_++;
    node.syntaxKind = kind;
    node.children.push_back(child);
    return add(std::move(node));
}

FormatDocument DocumentBuilder::finish(DocId root) && {
    document_.root = root;
    return std::move(document_);
}

RenderedDocument DocumentRenderer::renderLayout(const FormatDocument& document) const {
    std::vector<FlatAtom> atoms;
    flatten(document, document.root, {}, atoms);
    auto options = solve(atoms, config_, {});
    auto result = renderAtoms(atoms, config_, options, true).rendered;
    normalizeRenderedText(result.text);
    if (result.text.empty() || result.text.back() != '\n')
        result.text.push_back('\n');
    return result;
}

RenderedDocument DocumentRenderer::renderAligned(const FormatDocument& document,
                                                 const RenderedDocument& layout) const {
    std::vector<FlatAtom> atoms;
    flatten(document, document.root, {}, atoms);
    RenderOptions options;
    options.breaks = layout.breaks;
    auto alignment = computeAlignment(layout, config_);
    options.padding = std::move(alignment.padding);
    options.alignedContinuations = std::move(alignment.continuations);
    options = solve(atoms, config_, std::move(options));
    auto result = renderAtoms(atoms, config_, options, true).rendered;
    normalizeRenderedText(result.text);
    if (result.text.empty() || result.text.back() != '\n')
        result.text.push_back('\n');
    return result;
}

} // namespace format
