/* === S Y N F I G ========================================================= */
/*!	\file layerduplicate.cpp
**	\brief Template File
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2008 Chris Moore
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "layerduplicate.h"

#include <synfig/context.h>
#include <synfig/general.h>
#include <synfig/layers/layer_pastecanvas.h>
#include <synfig/valuenodes/valuenode_bone.h>

#include <synfigapp/actions/layeradd.h>
#include <synfigapp/canvasinterface.h>
#include <synfigapp/localization.h>

#endif

using namespace etl;
using namespace synfig;
using namespace synfigapp;
using namespace Action;

/* === M A C R O S ========================================================= */

ACTION_INIT_NO_GET_LOCAL_NAME(Action::LayerDuplicate);
ACTION_SET_NAME(Action::LayerDuplicate,"LayerDuplicate");
ACTION_SET_LOCAL_NAME(Action::LayerDuplicate,N_("Duplicate Layer"));
ACTION_SET_TASK(Action::LayerDuplicate,"duplicate");
ACTION_SET_CATEGORY(Action::LayerDuplicate,Action::CATEGORY_LAYER);
ACTION_SET_PRIORITY(Action::LayerDuplicate,0);
ACTION_SET_VERSION(Action::LayerDuplicate,"0.0");

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/// Search for all duplicates of explicit layers (those set as action parameter) and implicit layers too (contents of layer of group type)
static void
traverse_layers(synfig::Layer::Handle layer, synfig::Layer::Handle cloned_layer, std::map<synfig::Layer::Handle, synfig::Layer::Handle>& cloned_layer_map);

/// Get value nodes that are special cases when duplicating
/// Attention: The order of returned value nodes MUST be deterministic!
/// \param layer where to search for special value nodes
/// \param canvas the Layer Duplicate action canvas
/// \param src_layer_canvas the canvas of layer that is being duplicated/cloned
static std::vector<ValueNode::RHandle>
get_special_layer_valuenodes(synfig::Layer::Handle layer, synfig::Canvas::Handle canvas, synfig::Canvas::Handle src_layer_canvas)
{
	std::vector<ValueNode::RHandle> valuenodes;
	if (layer->get_name() == "duplicate") {
		valuenodes.push_back(layer->dynamic_param_list().find("index")->second);
	} else if (layer->get_name() == "skeleton") {
		if (layer->get_canvas() != src_layer_canvas) {
			// I don't know why, but it's needed for updating cloned skeleton layer bone names
			layer->set_time(canvas->get_independent_context(), canvas->get_time());
		}

		ValueBase param_bones = layer->get_param("bones");
		if (!param_bones.is_valid()) {
			error(_("Skeleton layer without 'bones' parameter!"));
		} else {
			std::vector<Bone> bone_list = param_bones.get_list_of(Bone());
			const size_t num_bones = bone_list.size();

			ValueNode_Bone::Handle ref_bone = ValueNode_Bone::create(Bone(), src_layer_canvas);
			for (size_t i = 0; i < num_bones; i++) {
				ValueNode_Bone::LooseHandle bone_vn = ref_bone->find(bone_list[i].get_name());
				if (bone_vn) {
					valuenodes.push_back(bone_vn.get());
				}
			}
		}
	}
	return valuenodes;
}

/// Scan a ValueNode parameter tree and replaces special valuenodes with their respective duplicates (clones)
static void
do_replace_valuenodes(ValueNode::Handle vn, const std::pair<ValueNode::RHandle, ValueNode::RHandle>& vn_pair)
{
	if (!vn)
		warning(_("Can't replace a null valuenode"));

	if (auto const_vn = ValueNode_Const::Handle::cast_dynamic(vn)) {
		// Check and replace value with special cloned valuenode
		if (const_vn->get_type() == type_bone_valuenode) {
			if (const_vn->get_value().get(ValueNode_Bone::Handle()) == vn_pair.first)
				if (ValueNode_Bone::Handle bone_valuenode = ValueNode_Bone::Handle::cast_dynamic(vn_pair.second)) {
					ValueBase ret_vb(bone_valuenode);
					ret_vb.copy_properties_of(bone_valuenode);
					const_vn->set_value(ret_vb);
				}
		} else {
			// There isn't any other value node that can be placed into a ValueBase (see type_*)
		}
	} else if (auto link_vn = LinkableValueNode::Handle::cast_dynamic(vn)) {
		// Check and replace every link with special cloned valuenode
		for (int i = 0; i < link_vn->link_count(); i++)
		{
			if (link_vn->get_link(i) == vn_pair.first) {
				link_vn->set_link(i, vn_pair.second);
			} else {
				do_replace_valuenodes(link_vn->get_link(i), vn_pair);
			}
		}
	} else if (auto animated_vn = ValueNode_Animated::Handle::cast_dynamic(vn)) {
		// Check and replace every waypoint value with special cloned valuenode
		const ValueNode_Animated::WaypointList& list(animated_vn->waypoint_list());
		for (ValueNode_Animated::WaypointList::const_iterator iter = list.cbegin(); iter != list.cend(); ++iter)
		{
			do_replace_valuenodes(iter->get_value_node(), vn_pair);
		}
	}
}

/// Go up in Canvas tree to the first parent canvas that is not inline or root canvas
/// Valuenodes are exported to this canvas via Canvas::add_value_node()
static Canvas::Handle get_top_parent_if_inline_canvas(Canvas::Handle canvas)
{
	if(canvas->is_inline() && canvas->parent())
		return get_top_parent_if_inline_canvas(canvas->parent());
	return canvas;
}

/// Remove the layers that are inside an already listed group-kind layer, as they would be duplicated twice
static std::list<Layer::Handle>
remove_layers_inside_included_pastelayers(const std::list<Layer::Handle>& layer_list)
{
	std::vector<Layer::Handle> layerpastecanvas_list;
	for (const auto& layer : layer_list) {
		if (Layer_PasteCanvas* pastecanvas = dynamic_cast<Layer_PasteCanvas*>(layer.get())) {
			layerpastecanvas_list.push_back(layer);
		}
	}

	std::list<Layer::Handle> clean_layer_list;
	for (const Layer::Handle layer : layer_list) {
		bool is_inside_a_selected_pastecanvas = false;
		auto parent_paste_canvas = layer->get_parent_paste_canvas_layer();
		while (parent_paste_canvas) {
			if (std::find(layerpastecanvas_list.begin(), layerpastecanvas_list.end(), parent_paste_canvas) != layerpastecanvas_list.end()) {
				is_inside_a_selected_pastecanvas = true;
				break;
			}
			parent_paste_canvas = parent_paste_canvas->get_parent_paste_canvas_layer();
		}
		if (!is_inside_a_selected_pastecanvas)
			clean_layer_list.push_back(layer);
	}
	return clean_layer_list;
}

/* === M E T H O D S ======================================================= */

Action::LayerDuplicate::LayerDuplicate()
{
}

synfig::String
Action::LayerDuplicate::get_local_name()const
{
	return get_layer_descriptions(layers, _("Duplicate Layer"), _("Duplicate Layers"));
}

Action::ParamVocab
Action::LayerDuplicate::get_param_vocab()
{
	ParamVocab ret(Action::CanvasSpecific::get_param_vocab());

	ret.push_back(ParamDesc("layer",Param::TYPE_LAYER)
		.set_local_name(_("Layer"))
		.set_desc(_("Layer to be duplicated"))
		.set_supports_multiple()
	);

	return ret;
}

bool
Action::LayerDuplicate::is_candidate(const ParamList &x)
{
	return candidate_check(get_param_vocab(),x);
}

bool
Action::LayerDuplicate::set_param(const synfig::String& name, const Action::Param &param)
{
	if(name=="layer" && param.get_type()==Param::TYPE_LAYER)
	{
		layers.push_back(param.get_layer());

		return true;
	}

	return Action::CanvasSpecific::set_param(name,param);
}

bool
Action::LayerDuplicate::is_ready()const
{
	if(layers.empty())
		return false;
	return Action::CanvasSpecific::is_ready();
}

void
Action::LayerDuplicate::prepare()
{
	if(!first_time())
		return;

	// remove layers that would be duplicated twice
	std::list<Layer::Handle> clean_layer_list = remove_layers_inside_included_pastelayers(layers);

	// pair (original layer, cloned layer)
	std::map<synfig::Layer::Handle,synfig::Layer::Handle> cloned_layer_map;
	// pair (original special valuenode, cloned special valuenode)
	std::map<ValueNode::RHandle, ValueNode::RHandle> cloned_valuenode_map;
	// pair (canvas, last exported valuenode "Index #" -> Layer_Duplicate parameter: index )
	std::map<Canvas::LooseHandle, int> last_index;

	for(auto layer : clean_layer_list)
	{
		Canvas::Handle subcanvas(layer->get_canvas());

		// Find the iterator for the layer
		Canvas::iterator iter=find(subcanvas->begin(),subcanvas->end(),layer);

		// If we couldn't find the layer in the canvas, then bail
		if(*iter!=layer)
			throw Error(_("This layer doesn't exist anymore."));

		// If the subcanvas isn't the same as the canvas,
		// then it had better be an inline canvas. If not,
		// bail
		if(get_canvas()!=subcanvas && !subcanvas->is_inline())
			throw Error(_("This layer doesn't belong to this canvas anymore"));

		// todo: which canvas should we use?  subcanvas is the layer's canvas, get_canvas() is the canvas set in the action
		Layer::Handle new_layer(layer->clone(subcanvas, guid));

		{
			Action::Handle action(Action::create("LayerMove"));

			action->set_param("canvas",subcanvas);
			action->set_param("canvas_interface",get_canvas_interface());
			action->set_param("layer",new_layer);
			action->set_param("new_index",layers.front()->get_depth());

			add_action_front(action);
		}
		{
			Action::Handle action(Action::create("LayerAdd"));

			action->set_param("canvas",subcanvas);
			action->set_param("canvas_interface",get_canvas_interface());
			action->set_param("new",new_layer);

			add_action_front(action);
		}

		// automatically export the Index parameter of Duplicate layers when duplicating
		Canvas::Handle canvas_with_exported_valuenodes = get_top_parent_if_inline_canvas(subcanvas);
		auto last_index_iter = last_index.find(canvas_with_exported_valuenodes);
		int index = last_index_iter == last_index.end() ? 1 : last_index_iter->second;
		export_dup_nodes(new_layer, subcanvas, index);
		last_index[canvas_with_exported_valuenodes] = index;

		// include this layer and all of its inner layers in a list
		traverse_layers(layer, new_layer, cloned_layer_map);
	}

	// search cloned layers for special parameter valuenodes that need to be remapped to those cloned ones
	// Known cases are currently:
	// - Index of Duplicate Layer
	// - Bone list of Skeleton Layer
	for (auto& layer_pair : cloned_layer_map) {
		Canvas::LooseHandle src_layer_canvas = layer_pair.first->get_canvas();
		std::vector<ValueNode::RHandle> src_valuenodes = get_special_layer_valuenodes(layer_pair.first, get_canvas(), src_layer_canvas);
		std::vector<ValueNode::RHandle> cloned_valuenodes = get_special_layer_valuenodes(layer_pair.second, get_canvas(), src_layer_canvas);
		const size_t num_valuenodes = src_valuenodes.size();
		if (num_valuenodes != cloned_valuenodes.size()) {
			error(_("Internal error: get_special_valuenodes doesn't return same number of valuenodes: %zu x %zu"),
			      num_valuenodes, cloned_valuenodes.size());
			continue;
		}

		for (size_t i = 0; i < num_valuenodes; i++) {
			if (src_valuenodes[i] != cloned_valuenodes[i])
				cloned_valuenode_map[src_valuenodes[i]] = cloned_valuenodes[i];
		}
	}

	// fix special cases of cloned layer parameter or cloned valuenodes that are linked to original valuenodes instead of cloned ones
	replace_valuenodes(cloned_layer_map, cloned_valuenode_map);
}

void
Action::LayerDuplicate::export_dup_nodes(synfig::Layer::Handle layer, Canvas::Handle canvas, int &index)
{
	// automatically export the Index parameter of Duplicate layers when duplicating
	if (layer->get_name() == "duplicate")
		while (true)
		{
			String name = strprintf(_("Index %d"), index++);
			try
			{
				canvas->find_value_node(name, true);
			}
			catch (const Exception::IDNotFound&)
			{
				Action::Handle action(Action::create("ValueNodeAdd"));

				action->set_param("canvas",canvas);
				action->set_param("canvas_interface",get_canvas_interface());
				action->set_param("new",layer->dynamic_param_list().find("index")->second);
				action->set_param("name",name);

				add_action_front(action);

				break;
			}
		}
	else
	{
		Layer::ParamList param_list(layer->get_param_list());
		for (Layer::ParamList::const_iterator iter(param_list.begin())
				 ; iter != param_list.end()
				 ; iter++)
			if (layer->dynamic_param_list().count(iter->first)==0 && iter->second.get_type()==type_canvas)
			{
				Canvas::Handle subcanvas(iter->second.get(Canvas::Handle()));
				if (subcanvas && subcanvas->is_inline())
					for (IndependentContext iter = subcanvas->get_independent_context(); iter != subcanvas->end(); iter++)
						export_dup_nodes(*iter, canvas, index);
			}

		for (Layer::DynamicParamList::const_iterator iter(layer->dynamic_param_list().begin())
				 ; iter != layer->dynamic_param_list().end()
				 ; iter++)
			if (iter->second->get_type()==type_canvas)
			{
				Canvas::Handle canvas((*iter->second)(0).get(Canvas::Handle()));
				if (canvas->is_inline())
					//! \todo do we need to implement this?  and if so, shouldn't we check all canvases, not just the one at t=0s?
					warning("%s:%d not yet implemented - do we need to export duplicate valuenodes in dynamic canvas parameters?", __FILE__, __LINE__);
			}
	}
}

static void
traverse_layers(synfig::Layer::Handle layer, synfig::Layer::Handle cloned_layer, std::map<synfig::Layer::Handle, synfig::Layer::Handle>& cloned_layer_map)
{
	cloned_layer_map[layer] = cloned_layer;

	Layer::ParamList param_list(layer->get_param_list());
	for (Layer::ParamList::const_iterator iter(param_list.begin())
			 ; iter != param_list.end()
			 ; iter++)
		if (layer->dynamic_param_list().count(iter->first)==0 && iter->second.get_type()==type_canvas)
		{
			Canvas::Handle subcanvas(iter->second.get(Canvas::Handle()));
			auto cloned_subcanvas = cloned_layer->get_param_list().find(iter->first)->second.get(Canvas::Handle());
			if (subcanvas && subcanvas->is_inline())
				for (IndependentContext iter = subcanvas->get_independent_context(), cloned_iter = cloned_subcanvas->get_independent_context(); iter != subcanvas->end(); ++iter, ++cloned_iter)
					traverse_layers(*iter, *cloned_iter, cloned_layer_map);
		}

	for (Layer::DynamicParamList::const_iterator iter(layer->dynamic_param_list().begin())
			 ; iter != layer->dynamic_param_list().end()
			 ; iter++)
		if (iter->second->get_type()==type_canvas)
		{
			Canvas::Handle canvas((*iter->second)(0).get(Canvas::Handle()));
			if (canvas->is_inline())
				//! \todo do we need to implement this?  and if so, shouldn't we check all canvases, not just the one at t=0s?
				warning("%s:%d not yet implemented - do we need to export duplicate valuenodes in dynamic canvas parameters?", __FILE__, __LINE__);
		}
}

void
LayerDuplicate::replace_valuenodes(const std::map<synfig::Layer::Handle,synfig::Layer::Handle>& cloned_layer_map, const std::map<synfig::ValueNode::RHandle, synfig::ValueNode::RHandle>& cloned_valuenode_map)
{
	if (cloned_valuenode_map.empty())
		return;

	for (const auto& layer_pair : cloned_layer_map) {

		auto cloned_layer = layer_pair.second;

		// disconnect_dynamic_param/connect_dynamic_param can change dynamic_param_list() while iterating
		// which makes iter invalid, so we create a copy of dynamic_param_list() first
		auto param_list = cloned_layer->dynamic_param_list();

		for (auto iter=param_list.cbegin();iter!=param_list.cend();++iter)
		{
			for (const auto& vn_pair : cloned_valuenode_map) {
				if (iter->second == vn_pair.first) {
					cloned_layer->disconnect_dynamic_param(iter->first);
					cloned_layer->connect_dynamic_param(iter->first, vn_pair.second);
				} else {
					do_replace_valuenodes(iter->second, vn_pair);
				}
			}
		}
	}
}
