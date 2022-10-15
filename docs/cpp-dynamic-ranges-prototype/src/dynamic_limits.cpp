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

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <numeric>
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

template <class Type>
vector<vector<Type>> cartesian(vector<vector<Type>>& v) {
  vector<vector<Type>> result;
  auto product = [](long long a, vector<Type>& b) { return a * b.size(); };
  const long long N = accumulate(v.begin(), v.end(), 1LL, product);
  vector<Type> u(v.size());
  for (long long n = 0; n < N; ++n) {
    lldiv_t q{n, 0};
    for (long long i = v.size() - 1; 0 <= i; --i) {
      q = div(q.quot, v[i].size());
      u[i] = v[i][q.rem];
    }
    result.push_back(u);
  }
  return result;
}

double query_feature_limits(GenApi::INode* feature_node, std::string limit) {
  if (intfIInteger == feature_node->GetPrincipalInterfaceType()) {
    if ("max" == limit) {
      return Pylon::CIntegerParameter(feature_node).GetMax();
    } else {
      return Pylon::CIntegerParameter(feature_node).GetMin();
    }
  } else {
    if ("max" == limit) {
      return Pylon::CFloatParameter(feature_node).GetMax();
    } else {
      return Pylon::CFloatParameter(feature_node).GetMin();
    }
  }
}

double check_for_feature_invalidators(
    GenApi::INode* feature_node, GenApi::INode* limit_node, std::string limit,
    unordered_map<std::string, GenApi::INode*>& invalidators) {
  double limit_under_all_settings = 0;
  GenICam::gcstring value;
  GenICam::gcstring attribute;

  if (!limit_node ||
      !limit_node->GetProperty("pInvalidator", value, attribute)) {
    limit_under_all_settings = query_feature_limits(feature_node, limit);
  } else {
    limit_node->GetProperty("pInvalidator", value, attribute);
    add_all_property_values(feature_node, std::string(value), invalidators);
  }

  return limit_under_all_settings;
}

class Actions {
 public:
  void virtual set_value() = 0;
};

template <class T, class V>
class ValueAction : public Actions {
 public:
  ValueAction(T param, V value) {
    this->param = param;
    this->value = value;
  }
  void set_value() override { this->param.SetValue(this->value); }

 private:
  T param;
  V value;
};

vector<vector<Actions*>> create_set_value_actions(
    vector<GenApi::INode*> node_list) {
  vector<vector<Actions*>> actions_list;

  for (const auto& node : node_list) {
    vector<Actions*> values;
    if (intfIBoolean == node->GetPrincipalInterfaceType()) {
      Pylon::CBooleanParameter param(node);
      values.push_back(
          new ValueAction<Pylon::CBooleanParameter, bool>(param, true));
      values.push_back(
          new ValueAction<Pylon::CBooleanParameter, bool>(param, false));
    } else if (intfIFloat == node->GetPrincipalInterfaceType()) {
      Pylon::CFloatParameter param(node);
      values.push_back(new ValueAction<Pylon::CFloatParameter, double>(
          param, param.GetMin()));
      values.push_back(new ValueAction<Pylon::CFloatParameter, double>(
          param, param.GetMax()));
    } else if (intfIInteger == node->GetPrincipalInterfaceType()) {
      Pylon::CIntegerParameter param(node);
      values.push_back(new ValueAction<Pylon::CIntegerParameter, int64_t>(
          param, param.GetMin()));
      values.push_back(new ValueAction<Pylon::CIntegerParameter, int64_t>(
          param, param.GetMax()));
    } else if (intfIEnumeration == node->GetPrincipalInterfaceType()) {
      Pylon::CEnumParameter param(node);
      GenApi::StringList_t symbolics;
      param.GetSymbolics(symbolics);
      for (const auto& symbolic : symbolics) {
        values.push_back(
            new ValueAction<Pylon::CEnumParameter, Pylon::String_t>(param,
                                                                    symbolic));
      }
    } else {
      cout << node->GetName() << " " << node->GetPrincipalInterfaceType()
           << endl;
    }
    actions_list.push_back(values);
  }

  return actions_list;
}

void find_limits(GenApi::INode* feature_node, double& min_result,
                 double& max_result,
                 vector<GenApi::INode*>& invalidators_result) {
  unordered_map<std::string, GenApi::INode*> invalidators;
  double maximum_under_all_settings = 0;
  double minimum_under_all_settings = 0;

  /* Find the maximum value of a feature under the influence of other elements
   * of the nodemap */
  GenApi::INode* pmax_node = find_limit_node(feature_node, "pMax");
  maximum_under_all_settings = check_for_feature_invalidators(
      feature_node, pmax_node, "max", invalidators);

  /* Find the minimum value of a feature under the influence of other elements
   * of the nodemap */
  GenApi::INode* pmin_node = find_limit_node(feature_node, "pMin");
  minimum_under_all_settings = check_for_feature_invalidators(
      feature_node, pmin_node, "min", invalidators);

  /* Return if no invalidator nodes found */
  if (invalidators.empty()) {
    min_result = minimum_under_all_settings;
    max_result = maximum_under_all_settings;
    invalidators_result.clear();
    return;
  }

  /* Find all features that control the node */
  vector<GenApi::INode*> parent_invalidators;
  for (const auto& inv : invalidators) {
    vector<GenApi::INode*> parent_features = find_parent_features(inv.second);
    parent_invalidators.insert(parent_invalidators.end(),
                               parent_features.begin(), parent_features.end());
  }

  /* Filter parent invalidators to only available ones */
  vector<GenApi::INode*> available_parent_inv =
      get_available_features(parent_invalidators);

  /* Create list of extreme value settings per invalidator */
  vector<vector<Actions*>> actions_list =
      create_set_value_actions(available_parent_inv);

  /* Create list of all possible setting permutations and execute them all */
  auto action_list_permutations = cartesian(actions_list);
  vector<double> min_values;
  vector<double> max_values;
  for (const auto& actions : action_list_permutations) {
    for (const auto& action : actions) {
      try {
        action->set_value();
      } catch (const GenericException& e) {
        continue;
      }
    }
    /* Capture min and max values after all setting are applied*/
    min_values.push_back(query_feature_limits(feature_node, "min"));
    max_values.push_back(query_feature_limits(feature_node, "max"));
  }

  /* Get the max and min values under all settings executed*/
  minimum_under_all_settings =
      *std::min_element(min_values.begin(), min_values.end());
  maximum_under_all_settings =
      *std::max_element(max_values.begin(), max_values.end());

  /* Clean up */
  for (const auto& actions : actions_list) {
    for (const auto& action : actions) {
      delete action;
    }
  }

  min_result = minimum_under_all_settings;
  max_result = maximum_under_all_settings;
  invalidators_result = available_parent_inv;
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

  /* Walk over all features of the tree */
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

    /* Walk down all categories */
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
  int exitCode = 0;

  PylonInitialize();

  double min_result;
  double max_result;
  vector<GenApi::INode*> invalidators_result;

  try {
    CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
    camera.Open();
    for (const auto& node : walk_nodes(camera)) {
      find_limits(node, min_result, max_result, invalidators_result);
      cout << node->GetName() << " "
           << "(" << min_result << ", " << max_result << ", [ ";
      for (const auto& i : invalidators_result) {
        cout << i->GetName() << " ";
      }
      cout << "])\n";
    }
    camera.Close();
  } catch (const GenericException& e) {
    cout << "An exception occurred." << endl << e.GetDescription() << endl;
  }

  PylonTerminate();

  return exitCode;
}
