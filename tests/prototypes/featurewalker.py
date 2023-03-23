# Copyright (C) 2023 Basler AG
#
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#     1. Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#     2. Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#     3. Neither the name of the copyright holder nor the names of
#        its contributors may be used to endorse or promote products
#        derived from this software without specific prior written
#        permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.
#
from queue import Queue
from typing import List
import pypylon.genicam as geni
import itertools


class FeatureSelector(object):
    def __init__(self, selector_node, value):
        self.selector_node = selector_node
        self.value = value

    def __repr__(self):
        return f"SEL<{self.selector_node.Node.GetName()}/{self.value}>"


class FeatureNode(object):
    """
    a feature node is the break down of each camera feature
    under each specific selector
    """

    def __init__(
        self,
        node,
        selectors: List[List[FeatureSelector,]] = None,
    ):
        self.node = node
        if selectors is None:
            selectors = []
        self.selectors = selectors

    def __repr__(self):
        repr = f"FeatureNode {self.node.Node.GetName()} "
        if self.selectors:
            for sel in self.selectors:
                repr += " " + str(sel)
        else:
            repr += "direct"
        return repr


def get_node_selectors(node):
    selectors = []
    for s in node.Node.GetSelectingFeatures():
        selector_entries = []
        if type(s) == geni.IInteger:
            for idx in range(s.GetMin(), s.GetMax() + s.GetInc(), s.GetInc()):
                selector_entries.append(FeatureSelector(s, idx))
        elif type(s) == geni.IEnumeration:
            for enum_val in s.Symbolics:
                if geni.IsImplemented(s.GetEntryByName(enum_val)):
                    selector_entries.append(FeatureSelector(s, enum_val))

        selectors.append(selector_entries)

    return selectors


# feature walker
# walks down feature tree to get all implemented features
def get_feature_nodes(nodemap, with_selectors: bool, only_implemented: bool):
    import queue

    worklist = queue.Queue()
    worklist.put(nodemap.GetNode("Root"))

    node_list = []
    while not worklist.empty():
        current_node = worklist.get()

        if only_implemented and not geni.IsImplemented(current_node):
            # print(f"skip not implemented node {current_node.Node.Name}")
            continue

        if len(current_node.Node.GetSelectedFeatures()):
            # print(f"skip selector {current_node.Node.Name}")
            continue

        if type(current_node) == geni.ICategory:
            # push children of category
            children = current_node.Node.GetChildren()
            for c in children:
                worklist.put(c)
            continue

        if type(current_node) in (geni.ICommand, geni.IRegister):
            continue

        # loop over all selector options of current node
        if with_selectors and len(current_node.Node.GetSelectingFeatures()):
            node_selectors = get_node_selectors(current_node)
            if len(list(itertools.product(*node_selectors))) > 1:
                for selectors in itertools.product(*node_selectors):
                    node_list.append(FeatureNode(current_node, selectors))
            else:
                # for features with only a single selector output the direct form too
                node_list.append(FeatureNode(current_node))
        else:
            node_list.append(FeatureNode(current_node))

    return node_list
