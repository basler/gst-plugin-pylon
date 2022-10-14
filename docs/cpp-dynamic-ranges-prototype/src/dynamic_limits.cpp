// Copyright (C) 2022 Basler AG

// ExtractCameraProperties.cpp
// extract feature candidates without opening the device

// Include files to use the pylon API.
#include <pylon/PylonIncludes.h>

#include <functional>
#include <queue>

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using GenApi objects.
using namespace GenApi;

// Namespace for using cout.
using namespace std;

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

GenApi::INode* find_limit_node(GenApi::INode* feature_node,
                               const GenICam::gcstring limit) {
  GenApi::INode* limit_node = NULL;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  if (feature_node->GetProperty(limit, value, attribute)) {
    limit_node = feature_node->GetNodeMap()->GetNode(value);
  } else if (feature_node->GetProperty("pValue", value, attribute)) {
    limit_node =
        find_limit_node(feature_node->GetNodeMap()->GetNode(value), limit);
  }
  return limit_node;
}

vector<GenApi::INode*> find_parent_features(GenApi::INode* feature_node) {
  vector<GenApi::INode*> parent_features;

  if (feature_node->IsFeature()) {
    parent_features.push_back(feature_node);
  } else {
    GenApi::NodeList_t parents;
    feature_node->GetParents(parents);
    for (const auto& parent : parents) {
      vector<GenApi::INode*> grandparents = find_parent_features(parent);
      parent_features.insert(parent_features.end(), grandparents.begin(),
                             grandparents.end());
    }
  }
  return parent_features;
}

bool is_node_integer(GenApi::INode* feature_node) {
  if (intfIInteger == feature_node->GetPrincipalInterfaceType()) {
    return true;
  } else {
    return false;
  }
}

void add_all_property_values(
    GenApi::INode* feature_node, std::string value,
    unordered_map<std::string, GenApi::INode*>& invalidators) {
  string delimiter = "\t";

  size_t pos = 0;
  std::string token;

  while ((pos = value.find(delimiter)) != std::string::npos) {
    token = value.substr(0, pos);
    if (invalidators.find(token) == invalidators.end()) {
      invalidators[token] = feature_node->GetNodeMap()->GetNode(token.c_str());
    }
    value.erase(0, pos + delimiter.length());
  }

  if (invalidators.find(value) == invalidators.end()) {
    invalidators[value] = feature_node->GetNodeMap()->GetNode(value.c_str());
  }
}

vector<GenApi::INode*> get_available_features(
    vector<GenApi::INode*> feature_list) {
  vector<GenApi::INode*> available_features;
  for (const auto& feature : feature_list) {
    if (GenApi::IsAvailable(feature)) {
      available_features.push_back(feature);
    }
  }
  return available_features;
}

class Actions {
 public:
  void virtual set_value() = 0;

 protected:
  GenApi::INode* node;
};

class BoolTrueAction : public Actions {
 public:
  BoolTrueAction(GenApi::INode* node) { this->node = node; }
  void set_value() override {
    Pylon::CBooleanParameter param(this->node);
    param.SetValue(true);
  }
};

class BoolFalseAction : public Actions {
 public:
  BoolFalseAction(GenApi::INode* node) { this->node = node; }
  void set_value() override {
    Pylon::CBooleanParameter param(this->node);
    param.SetValue(false);
  }
};

class IntMinAction : public Actions {
 public:
  IntMinAction(GenApi::INode* node) { this->node = node; }
  void set_value() override {
    Pylon::CIntegerParameter param(this->node);
    param.SetValue(param.GetMin());
  }
};

class IntMaxAction : public Actions {
 public:
  IntMaxAction(GenApi::INode* node) { this->node = node; }
  void set_value() override {
    Pylon::CIntegerParameter param(this->node);
    param.SetValue(param.GetMax());
  }
};

class FloatMinAction : public Actions {
 public:
  FloatMinAction(GenApi::INode* node) { this->node = node; }
  void set_value() override {
    Pylon::CFloatParameter param(this->node);
    param.SetValue(param.GetMin());
  }
};

class FloatMaxAction : public Actions {
 public:
  FloatMaxAction(GenApi::INode* node) { this->node = node; }
  void set_value() override {
    Pylon::CFloatParameter param(this->node);
    param.SetValue(param.GetMax());
  }
};

class EnumAction : public Actions {
 public:
  EnumAction(GenApi::INode* node, const Pylon::String_t& symbolic) {
    this->node = node;
    this->symbolic = symbolic;
  }
  void set_value() override {
    Pylon::CEnumParameter param(this->node);
    param.SetValue(this->symbolic);
  }

 private:
  Pylon::String_t symbolic;
};

vector<vector<Actions*>> create_set_value_actions(
    vector<GenApi::INode*> node_list) {
  vector<vector<Actions*>> actions_list;

  for (const auto& node : node_list) {
    if (intfIBoolean == node->GetPrincipalInterfaceType()) {
      vector<Actions*> values;
      BoolTrueAction bool_true(node);
      BoolFalseAction bool_false(node);
      values.push_back(&bool_true);
      values.push_back(&bool_false);
      actions_list.push_back(values);
    } else if (intfIFloat == node->GetPrincipalInterfaceType()) {
      vector<Actions*> values;
      FloatMinAction float_min(node);
      FloatMaxAction float_max(node);
      values.push_back(&float_min);
      values.push_back(&float_max);
      actions_list.push_back(values);
    } else if (intfIInteger == node->GetPrincipalInterfaceType()) {
      vector<Actions*> values;
      IntMinAction int_min(node);
      IntMaxAction int_max(node);
      values.push_back(&int_min);
      values.push_back(&int_max);
      actions_list.push_back(values);
    } else if (intfIEnumeration == node->GetPrincipalInterfaceType()) {
      vector<Actions*> values;
      Pylon::CEnumParameter param(node);
      GenApi::StringList_t symbolics;
      param.GetSymbolics(symbolics);
      for (const auto& symbolic : symbolics) {
        EnumAction enum_action(node, symbolic);
        values.push_back(&enum_action);
      }
      actions_list.push_back(values);
    } else {
      cout << node->GetName() << " " << node->GetPrincipalInterfaceType()
           << endl;
    }
  }

  return actions_list;
}

void find_limits(GenApi::INode* feature_node) {
  unordered_map<std::string, GenApi::INode*> invalidators;
  int64_t maximum_under_all_settings = 0;
  int64_t minimum_under_all_settings = 0;

  GenICam::gcstring value;
  GenICam::gcstring attribute;

  GenApi::INode* pmax_node = find_limit_node(feature_node, "pMax");
  if (!pmax_node || !pmax_node->GetProperty("pInvalidator", value, attribute)) {
    if (is_node_integer(feature_node)) {
      maximum_under_all_settings =
          Pylon::CIntegerParameter(feature_node).GetMax();
    } else {
      maximum_under_all_settings =
          Pylon::CFloatParameter(feature_node).GetMax();
    }
  } else {
    pmax_node->GetProperty("pInvalidator", value, attribute);
    add_all_property_values(feature_node, std::string(value), invalidators);
  }

  GenApi::INode* pmin_node = find_limit_node(feature_node, "pMin");
  if (!pmin_node || !pmin_node->GetProperty("pInvalidator", value, attribute)) {
    if (is_node_integer(feature_node)) {
      minimum_under_all_settings =
          Pylon::CIntegerParameter(feature_node).GetMin();
    } else {
      minimum_under_all_settings =
          Pylon::CFloatParameter(feature_node).GetMin();
    }
  } else {
    pmin_node->GetProperty("pInvalidator", value, attribute);
    add_all_property_values(feature_node, std::string(value), invalidators);
  }

  /* Return if no invalidator nodes found */
  if (invalidators.empty()) {
    return;
  }

  vector<GenApi::INode*> invalidator_feature;
  for (const auto& inv : invalidators) {
    vector<GenApi::INode*> parent_features = find_parent_features(inv.second);
    invalidator_feature.insert(invalidator_feature.end(),
                               parent_features.begin(), parent_features.end());
  }

  vector<GenApi::INode*> available_inv_feature =
      get_available_features(invalidator_feature);

  cout << "\nAfter filtering" << endl;
  for (const auto& i : available_inv_feature) {
    cout << i->GetName() << endl;
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

    if (GenApi::IsAvailable(node)) {
      if (has_ranges(node)) {
        node_list.push_back(node);
      }
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
    camera.Open();
    for (const auto& node : walk_nodes(camera)) {
      find_limits(node);
    }
    camera.Close();
  } catch (const GenericException& e) {
    // Error handling.
    cout << "An exception occurred." << endl << e.GetDescription() << endl;
  }

  // Releases all pylon resources.
  PylonTerminate();

  return exitCode;
}
