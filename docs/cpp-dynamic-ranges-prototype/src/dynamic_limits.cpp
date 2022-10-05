// Copyright (C) 2022 Basler AG

// ExtractCameraProperties.cpp
// extract feature candidates without opening the device

// Include files to use the pylon API.
#include <pylon/PylonIncludes.h>

#include <queue>

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using GenApi objects.
using namespace GenApi;

// Namespace for using cout.
using namespace std;

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

GenApi::INode* find_limit_node(GenApi::INode* feature_node,
                               const GenICam::gcstring limit) {
  GenApi::INode* limit_node;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  if (feature_node->GetProperty(limit, value, attribute)) {
    limit_node = feature_node->GetNodeMap()->GetNode(value);
  } else if (feature_node->GetProperty(limit, value, attribute)) {
    limit_node =
        find_limit_node(feature_node->GetNodeMap()->GetNode(value), limit);
  } else {
    return limit_node;
  }
}

/* Feature Walker */
bool has_ranges(INode* node) {
  bool add_to_list = false;

  switch (node->GetPrincipalInterfaceType()) {
    case intfIInteger:
      add_to_list = true;
      break;
    case intfIFloat:
      add_to_list = true;
      break;
    case intfIBoolean:
      break;
    case intfIString:
      break;
    case intfIEnumeration:
      break;
    case intfICommand:
      break;
    case intfIRegister:
      break;
    default:
      break;
  }

  return add_to_list;
}

vector<INode*> walk_nodes(CInstantCamera& camera) {
  INodeMap& nodemap = camera.GetNodeMap();
  vector<INode*> node_list;

  // walk over all features of the tree
  INode* root_node = nodemap.GetNode("Root");
  auto worklist = std::queue<INode*>();

  worklist.push(root_node);
  while (!worklist.empty()) {
    auto node = worklist.front();
    worklist.pop();

    if (has_ranges(node)) {
      node_list.push_back(node);
    }

    // walk down all categories
    auto category_node = dynamic_cast<ICategory*>(node);
    if (category_node) {
      FeatureList_t features;
      category_node->GetFeatures(features);
      for (auto const& f : features) {
        worklist.push(f->GetNode());
      }
    }
  }

  return node_list;
}

int main(int /*argc*/, char* /*argv*/ []) {
  // The exit code of the sample application.
  int exitCode = 0;

  // Before using any pylon methods, the pylon runtime must be initialized.
  PylonInitialize();

  try {
    // Create an instant camera object with the camera found first.
    CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
    for (const auto& node : walk_nodes(camera)) {
      cout << node->GetName() << endl;
    }
  } catch (const GenericException& e) {
    // Error handling.
    cerr << "An exception occurred." << endl << e.GetDescription() << endl;
    exitCode = 1;
  }

  // Releases all pylon resources.
  PylonTerminate();

  return exitCode;
}
