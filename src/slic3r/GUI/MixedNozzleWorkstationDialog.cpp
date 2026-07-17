#include "MixedNozzleWorkstationDialog.hpp"

#include "I18N.hpp"
#include "libslic3r/MixedLayerHeight.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Slic3r { namespace GUI {

namespace {

int config_int(const DynamicPrintConfig& config, const char* key, const int fallback)
{
    const auto* opt = config.option<ConfigOptionInt>(key);
    return opt != nullptr ? opt->value : fallback;
}

bool config_bool(const DynamicPrintConfig& config, const char* key, const bool fallback)
{
    const auto* opt = config.option<ConfigOptionBool>(key);
    return opt != nullptr ? opt->value : fallback;
}

double config_float(const DynamicPrintConfig& config, const char* key, const double fallback)
{
    const auto* opt = config.option<ConfigOptionFloat>(key);
    return opt != nullptr ? opt->value : fallback;
}

MixedNozzleMode config_mixed_nozzle_mode(const DynamicPrintConfig& config)
{
    const auto* opt = config.option<ConfigOptionEnum<MixedNozzleMode>>("mixed_nozzle_mode");
    return opt != nullptr ? opt->value : MixedNozzleMode::SameLayer;
}

wxStaticText* add_wrapped_text(wxWindow* parent, wxSizer* sizer, const wxString& text, const int width)
{
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    label->Wrap(width);
    sizer->Add(label, 0, wxEXPAND | wxALL, parent->FromDIP(10));
    return label;
}

void add_field(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label, wxWindow* ctrl)
{
    auto* text = new wxStaticText(parent, wxID_ANY, label);
    grid->Add(text, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, parent->FromDIP(8));
    grid->Add(ctrl, 1, wxEXPAND | wxBOTTOM, parent->FromDIP(8));
}

} // namespace

MixedNozzleWorkstationDialog::MixedNozzleWorkstationDialog(wxWindow* parent,
                                                           const DynamicPrintConfig& print_config,
                                                           const DynamicPrintConfig& printer_config,
                                                           const std::vector<std::string>& filament_names,
                                                           const std::vector<int>& physical_tool_indices)
    : DPIDialog(parent, wxID_ANY, _L("Mixed Nozzle Workstation"), wxDefaultPosition, wxSize(parent->FromDIP(620), parent->FromDIP(620)), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_print_config(print_config)
    , m_printer_config(printer_config)
    , m_filament_names(filament_names)
    , m_physical_tool_indices(physical_tool_indices)
{
    if (const auto* opt = printer_config.option<ConfigOptionFloats>("nozzle_diameter"))
        m_nozzle_diameters = opt->values;
    if (const auto* opt = printer_config.option<ConfigOptionFloats>("max_layer_height"))
        m_max_layer_heights = opt->values;

    const size_t nozzle_slot_count = m_nozzle_diameters.empty() ? m_filament_names.size() : m_nozzle_diameters.size();
    m_tool_count = int(std::max<size_t>(1, nozzle_slot_count));
    if (m_nozzle_diameters.size() < size_t(m_tool_count))
        m_nozzle_diameters.resize(m_tool_count, 0.4);
    if (m_max_layer_heights.size() < size_t(m_tool_count)) {
        const double fallback_max_layer_height = m_max_layer_heights.empty() ? 0. : m_max_layer_heights.front();
        m_max_layer_heights.resize(m_tool_count, fallback_max_layer_height);
    }
    if (m_filament_names.size() < size_t(m_tool_count)) {
        const size_t old_size = m_filament_names.size();
        m_filament_names.resize(m_tool_count);
        for (size_t i = old_size; i < m_filament_names.size(); ++i)
            m_filament_names[i] = "Filament " + std::to_string(i + 1);
    }
    if (m_physical_tool_indices.size() < size_t(m_tool_count)) {
        const size_t old_size = m_physical_tool_indices.size();
        m_physical_tool_indices.resize(m_tool_count);
        for (size_t i = old_size; i < m_physical_tool_indices.size(); ++i)
            m_physical_tool_indices[i] = int(i);
    }

    build_ui();
    load_from_config();
    update_validation();
}

void MixedNozzleWorkstationDialog::build_ui()
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* notebook = new wxNotebook(this, wxID_ANY);

    auto* quick_panel = new wxPanel(notebook);
    auto* quick_sizer = new wxBoxSizer(wxVERTICAL);
    m_strategy_choice = new wxChoice(quick_panel, wxID_ANY);
    // TRN: These are mixed-nozzle preset strategies, not color mixing strategies.
    m_strategy_choice->Append(_L("Same layer: feature tools and line widths"));
    m_strategy_choice->Append(_L("Mixed layer: sparse infill only"));
    m_strategy_choice->Append(_L("Mixed layer: sparse infill + inner walls"));
    m_strategy_choice->Append(_L("Mixed layer: full internal features (experimental)"));
    quick_sizer->Add(m_strategy_choice, 0, wxEXPAND | wxALL, quick_panel->FromDIP(10));
    add_wrapped_text(quick_panel, quick_sizer,
                     _L("Choose the high-level mixed nozzle strategy. Same-layer mode keeps one layer height. Mixed-layer modes keep outer walls fine and combine selected internal features for the coarse nozzle."),
                     quick_panel->FromDIP(540));
    quick_panel->SetSizer(quick_sizer);
    notebook->AddPage(quick_panel, _L("Quick Plan"));

    auto* map_panel = new wxPanel(notebook);
    auto* map_sizer = new wxBoxSizer(wxVERTICAL);
    auto* map_grid = new wxFlexGridSizer(4, 0, map_panel->FromDIP(8));
    map_grid->AddGrowableCol(3, 1);
    map_grid->Add(new wxStaticText(map_panel, wxID_ANY, _L("Logical tool")), 0, wxBOTTOM, map_panel->FromDIP(6));
    map_grid->Add(new wxStaticText(map_panel, wxID_ANY, _L("Physical head")), 0, wxBOTTOM, map_panel->FromDIP(6));
    map_grid->Add(new wxStaticText(map_panel, wxID_ANY, _L("Nozzle")), 0, wxBOTTOM, map_panel->FromDIP(6));
    map_grid->Add(new wxStaticText(map_panel, wxID_ANY, _L("Filament")), 0, wxBOTTOM, map_panel->FromDIP(6));
    for (int tool = 1; tool <= m_tool_count; ++tool) {
        map_grid->Add(new wxStaticText(map_panel, wxID_ANY, wxString::Format("T%d", tool - 1)), 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, map_panel->FromDIP(4));
        map_grid->Add(new wxStaticText(map_panel, wxID_ANY, physical_tool_label(tool)), 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, map_panel->FromDIP(4));
        map_grid->Add(new wxStaticText(map_panel, wxID_ANY, wxString::Format("%.2f mm", nozzle_diameter(tool))), 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, map_panel->FromDIP(4));
        map_grid->Add(new wxStaticText(map_panel, wxID_ANY, wxString::FromUTF8(m_filament_names[tool - 1])), 1, wxEXPAND | wxBOTTOM, map_panel->FromDIP(4));
    }
    map_sizer->Add(map_grid, 0, wxEXPAND | wxALL, map_panel->FromDIP(10));
    add_wrapped_text(map_panel, map_sizer,
                     _L("Nozzle diameters are read from native Printer settings > Extruder. The workstation writes logical slicer tools. U1 physical head order is shown only as a reference and does not change the printer mapping table."),
                     map_panel->FromDIP(540));
    map_panel->SetSizer(map_sizer);
    notebook->AddPage(map_panel, _L("Nozzle Map"));

    auto* feature_panel = new wxPanel(notebook);
    auto* feature_sizer = new wxBoxSizer(wxVERTICAL);
    auto* feature_grid = new wxFlexGridSizer(2, 0, feature_panel->FromDIP(8));
    feature_grid->AddGrowableCol(1, 1);
    m_outer_tool = new wxChoice(feature_panel, wxID_ANY);
    m_inner_wall_tool = new wxChoice(feature_panel, wxID_ANY);
    m_sparse_infill_tool = new wxChoice(feature_panel, wxID_ANY);
    m_solid_infill_tool = new wxChoice(feature_panel, wxID_ANY);
    m_outer_tool->Append(_L("Default (use inner wall tool)"));
    for (int tool = 1; tool <= m_tool_count; ++tool) {
        const wxString label = tool_label(tool);
        m_outer_tool->Append(label);
        m_inner_wall_tool->Append(label);
        m_sparse_infill_tool->Append(label);
        m_solid_infill_tool->Append(label);
    }
    add_field(feature_panel, feature_grid, _L("Outer wall"), m_outer_tool);
    add_field(feature_panel, feature_grid, _L("Inner wall"), m_inner_wall_tool);
    add_field(feature_panel, feature_grid, _L("Sparse infill"), m_sparse_infill_tool);
    add_field(feature_panel, feature_grid, _L("Internal solid infill"), m_solid_infill_tool);
    feature_sizer->Add(feature_grid, 0, wxEXPAND | wxALL, feature_panel->FromDIP(10));
    feature_panel->SetSizer(feature_sizer);
    notebook->AddPage(feature_panel, _L("Feature Assignment"));

    auto* layer_panel = new wxPanel(notebook);
    auto* layer_sizer = new wxBoxSizer(wxVERTICAL);
    m_combine_sparse_infill = new wxCheckBox(layer_panel, wxID_ANY, _L("Combine sparse infill"));
    m_combine_inner_wall = new wxCheckBox(layer_panel, wxID_ANY, _L("Combine inner walls"));
    m_combine_internal_solid = new wxCheckBox(layer_panel, wxID_ANY, _L("Combine internal solid infill"));
    m_auto_coarse_layer_height = new wxCheckBox(layer_panel, wxID_ANY, _L("Auto coarse layer height"));
    m_coarse_layer_height = new wxSpinCtrlDouble(layer_panel, wxID_ANY);
    m_coarse_layer_height->SetRange(0., 2.);
    m_coarse_layer_height->SetIncrement(0.01);
    m_coarse_layer_height->SetDigits(2);
    for (wxCheckBox* cb : { m_combine_sparse_infill, m_combine_inner_wall, m_combine_internal_solid, m_auto_coarse_layer_height })
        layer_sizer->Add(cb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, layer_panel->FromDIP(10));
    auto* height_grid = new wxFlexGridSizer(2, 0, layer_panel->FromDIP(8));
    height_grid->AddGrowableCol(1, 1);
    add_field(layer_panel, height_grid, _L("Manual coarse layer height"), m_coarse_layer_height);
    layer_sizer->Add(height_grid, 0, wxEXPAND | wxALL, layer_panel->FromDIP(10));
    layer_panel->SetSizer(layer_sizer);
    notebook->AddPage(layer_panel, _L("Layer Combining"));

    auto* validation_panel = new wxPanel(notebook);
    auto* validation_sizer = new wxBoxSizer(wxVERTICAL);
    m_validation_text = add_wrapped_text(validation_panel, validation_sizer, "", validation_panel->FromDIP(540));
    validation_panel->SetSizer(validation_sizer);
    notebook->AddPage(validation_panel, _L("Validation"));

    root->Add(notebook, 1, wxEXPAND | wxALL, FromDIP(8));

    auto* buttons = new wxStdDialogButtonSizer();
    buttons->AddButton(new wxButton(this, wxID_CANCEL, _L("Cancel")));
    buttons->AddButton(new wxButton(this, wxID_OK, _L("OK")));
    buttons->Realize();
    root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    SetSizer(root);

    m_strategy_choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        apply_strategy(static_cast<Strategy>(m_strategy_choice->GetSelection()));
        update_validation();
    });

    for (wxChoice* choice : { m_outer_tool, m_inner_wall_tool, m_sparse_infill_tool, m_solid_infill_tool })
        choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { update_validation(); });
    for (wxCheckBox* cb : { m_combine_sparse_infill, m_combine_inner_wall, m_combine_internal_solid })
        cb->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { update_validation(); });
    m_auto_coarse_layer_height->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        update_manual_height_state();
        update_validation();
    });
    m_coarse_layer_height->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { update_validation(); });
    m_coarse_layer_height->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { update_validation(); });
}

void MixedNozzleWorkstationDialog::load_from_config()
{
    select_tool(m_outer_tool, config_int(m_print_config, "outer_wall_filament", 0), true);
    select_tool(m_inner_wall_tool, config_int(m_print_config, "wall_filament", 1));
    select_tool(m_sparse_infill_tool, config_int(m_print_config, "sparse_infill_filament", 1));
    select_tool(m_solid_infill_tool, config_int(m_print_config, "solid_infill_filament", 1));

    m_combine_sparse_infill->SetValue(config_bool(m_print_config, "mixed_nozzle_sparse_infill_combination", true));
    m_combine_inner_wall->SetValue(config_bool(m_print_config, "mixed_nozzle_inner_wall_combination", false));
    m_combine_internal_solid->SetValue(config_bool(m_print_config, "mixed_nozzle_internal_solid_infill_combination", false));
    m_auto_coarse_layer_height->SetValue(config_bool(m_print_config, "mixed_nozzle_auto_coarse_layer_height", true));
    m_coarse_layer_height->SetValue(config_float(m_print_config, "mixed_nozzle_coarse_layer_height", 0.));

    Strategy strategy = Strategy::SameLayer;
    if (config_mixed_nozzle_mode(m_print_config) == MixedNozzleMode::MixedLayer) {
        if (m_combine_internal_solid->GetValue())
            strategy = Strategy::FullInternalExperimental;
        else if (m_combine_inner_wall->GetValue())
            strategy = Strategy::SparseAndInnerWall;
        else
            strategy = Strategy::SparseOnly;
    }
    m_strategy_choice->SetSelection(int(strategy));
    update_manual_height_state();
}

void MixedNozzleWorkstationDialog::apply_strategy(const Strategy strategy)
{
    const int fine_tool = recommended_fine_tool();
    const int coarse_tool = recommended_coarse_tool(fine_tool);

    select_tool(m_outer_tool, fine_tool, true);

    if (strategy == Strategy::SameLayer) {
        select_tool(m_inner_wall_tool, coarse_tool);
        select_tool(m_sparse_infill_tool, coarse_tool);
        select_tool(m_solid_infill_tool, coarse_tool);
        m_combine_sparse_infill->SetValue(false);
        m_combine_inner_wall->SetValue(false);
        m_combine_internal_solid->SetValue(false);
        return;
    }

    select_tool(m_sparse_infill_tool, coarse_tool);
    select_tool(m_inner_wall_tool, strategy == Strategy::SparseAndInnerWall || strategy == Strategy::FullInternalExperimental ?
                                   coarse_tool : fine_tool);
    select_tool(m_solid_infill_tool, strategy == Strategy::FullInternalExperimental ? coarse_tool : fine_tool);

    m_combine_sparse_infill->SetValue(true);
    m_combine_inner_wall->SetValue(strategy == Strategy::SparseAndInnerWall || strategy == Strategy::FullInternalExperimental);
    m_combine_internal_solid->SetValue(strategy == Strategy::FullInternalExperimental);
    m_auto_coarse_layer_height->SetValue(true);
    update_manual_height_state();
}

void MixedNozzleWorkstationDialog::apply_to_config(DynamicPrintConfig& print_config) const
{
    const Strategy strategy = static_cast<Strategy>(m_strategy_choice->GetSelection());
    const bool sparse_combined = strategy != Strategy::SameLayer && m_combine_sparse_infill->GetValue();
    const bool inner_combined  = strategy != Strategy::SameLayer && m_combine_inner_wall->GetValue();
    const bool solid_combined  = strategy != Strategy::SameLayer && m_combine_internal_solid->GetValue();
    const bool mixed_layer     = sparse_combined || inner_combined || solid_combined;
    print_config.set_key_value("mixed_nozzle_mode", new ConfigOptionEnum<MixedNozzleMode>(
        mixed_layer ? MixedNozzleMode::MixedLayer : MixedNozzleMode::SameLayer));
    print_config.set_key_value("mixed_nozzle_sparse_infill_combination", new ConfigOptionBool(sparse_combined));
    print_config.set_key_value("mixed_nozzle_inner_wall_combination", new ConfigOptionBool(inner_combined));
    print_config.set_key_value("mixed_nozzle_internal_solid_infill_combination", new ConfigOptionBool(solid_combined));
    print_config.set_key_value("mixed_nozzle_auto_coarse_layer_height", new ConfigOptionBool(m_auto_coarse_layer_height->GetValue()));
    print_config.set_key_value("mixed_nozzle_coarse_layer_height", new ConfigOptionFloat(m_coarse_layer_height->GetValue()));
    print_config.set_key_value("outer_wall_filament", new ConfigOptionInt(selected_tool(m_outer_tool, true)));
    print_config.set_key_value("wall_filament", new ConfigOptionInt(selected_tool(m_inner_wall_tool)));
    print_config.set_key_value("sparse_infill_filament", new ConfigOptionInt(selected_tool(m_sparse_infill_tool)));
    print_config.set_key_value("solid_infill_filament", new ConfigOptionInt(selected_tool(m_solid_infill_tool)));

    const int inner_tool = selected_tool(m_inner_wall_tool);
    const int outer_tool_config = selected_tool(m_outer_tool, true);
    const int outer_tool = outer_tool_config == 0 ? inner_tool : outer_tool_config;
    set_feature_line_width(print_config, "outer_wall_line_width", outer_tool, 1.10);
    set_feature_line_width(print_config, "inner_wall_line_width", inner_tool, 1.05);
    set_feature_line_width(print_config, "sparse_infill_line_width", selected_tool(m_sparse_infill_tool), 1.10);
    set_feature_line_width(print_config, "internal_solid_infill_line_width", selected_tool(m_solid_infill_tool), 1.05);
}

void MixedNozzleWorkstationDialog::update_validation()
{
    const Strategy strategy = static_cast<Strategy>(m_strategy_choice->GetSelection());
    const double fine_layer_height = config_float(m_print_config, "layer_height", 0.);
    const int inherited_outer_tool = selected_tool(m_inner_wall_tool);
    const int configured_outer_tool = selected_tool(m_outer_tool, true);
    const int outer_tool = configured_outer_tool == 0 ? inherited_outer_tool : configured_outer_tool;

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);

    const bool all_nozzles_equal = std::all_of(m_nozzle_diameters.begin(), m_nozzle_diameters.end(),
        [this](const double diameter) { return std::fabs(diameter - m_nozzle_diameters.front()) <= 1e-6; });
    if (m_tool_count > 1 && all_nozzles_equal) {
        out << (_L("All configured nozzle diameters are the same. Change Printer settings > Extruder N to enable mixed-nozzle width differences.").ToUTF8().data())
            << "\n\n";
    }

    out << (_L("Outer wall").ToUTF8().data()) << ": "
        << selected_tool_label(configured_outer_tool, inherited_outer_tool).ToUTF8().data()
        << " / " << nozzle_diameter(outer_tool) << " mm"
        << " / " << suggested_line_width(outer_tool, 1.10) << " mm\n";

    const auto append_assignment = [&](const char* name, const int tool, const double multiplier) {
        out << name << ": T" << (tool - 1)
            << " / " << nozzle_diameter(tool) << " mm"
            << " / " << suggested_line_width(tool, multiplier) << " mm\n";
    };

    append_assignment(_L("Inner wall").ToUTF8().data(), selected_tool(m_inner_wall_tool), 1.05);
    append_assignment(_L("Sparse infill").ToUTF8().data(), selected_tool(m_sparse_infill_tool), 1.10);
    append_assignment(_L("Internal solid infill").ToUTF8().data(), selected_tool(m_solid_infill_tool), 1.05);

    if (strategy == Strategy::SameLayer) {
        out << (_L("Same-layer mode is selected. The slicer will not merge feature layers.").ToUTF8().data());
        m_validation_text->SetLabel(wxString::FromUTF8(out.str()));
        m_validation_text->Wrap(FromDIP(540));
        Layout();
        return;
    }

    const double target_height = m_auto_coarse_layer_height->GetValue() ?
        0. : m_coarse_layer_height->GetValue();
    const auto append_feature = [&](const char* name, const int tool, const bool enabled) {
        if (!enabled)
            return;
        const double coarse_nozzle = nozzle_diameter(tool);
        const double coarse_max = max_layer_height(tool);
        const double target = m_auto_coarse_layer_height->GetValue() ?
                                  mixed_nozzle_auto_coarse_layer_height(coarse_nozzle, coarse_max) :
                                  target_height;
        const int ratio = mixed_nozzle_layer_height_ratio_from_target(fine_layer_height, target, coarse_nozzle, coarse_max);
        out << name << ": T" << (tool - 1) << " / " << coarse_nozzle << " mm, ";
        if (ratio >= 2)
            out << ratio << " x " << fine_layer_height << " mm -> " << (ratio * fine_layer_height) << " mm\n";
        else
            out << (_L("cannot combine: target height is less than two fine layers").ToUTF8().data()) << "\n";
    };

    const bool sparse_combined = m_combine_sparse_infill->GetValue();
    const bool inner_combined = m_combine_inner_wall->GetValue();
    const bool solid_combined = m_combine_internal_solid->GetValue();
    append_feature(_L("Sparse infill").ToUTF8().data(), selected_tool(m_sparse_infill_tool), sparse_combined);
    append_feature(_L("Inner wall").ToUTF8().data(), selected_tool(m_inner_wall_tool), inner_combined);
    append_feature(_L("Internal solid infill").ToUTF8().data(), selected_tool(m_solid_infill_tool), solid_combined);

    if (!sparse_combined && !inner_combined && !solid_combined)
        out << (_L("No layer-combined feature is enabled; applying this plan will use same-layer mode.").ToUTF8().data()) << "\n";

    out << "\n" << (_L("Top and bottom surfaces are not intentionally merged in this workstation version.").ToUTF8().data());
    m_validation_text->SetLabel(wxString::FromUTF8(out.str()));
    m_validation_text->Wrap(FromDIP(540));
    Layout();
}

void MixedNozzleWorkstationDialog::update_manual_height_state()
{
    m_coarse_layer_height->Enable(!m_auto_coarse_layer_height->GetValue());
}

int MixedNozzleWorkstationDialog::recommended_fine_tool() const
{
    int fine_tool = 1;
    double fine_nozzle = nozzle_diameter(1);
    for (int tool = 2; tool <= m_tool_count; ++tool) {
        const double diameter = nozzle_diameter(tool);
        if (diameter < fine_nozzle) {
            fine_tool = tool;
            fine_nozzle = diameter;
        }
    }
    return fine_tool;
}

int MixedNozzleWorkstationDialog::recommended_coarse_tool(const int fine_tool_1based) const
{
    int coarse_tool = std::clamp(fine_tool_1based, 1, m_tool_count);
    double coarse_nozzle = nozzle_diameter(coarse_tool);
    for (int tool = 1; tool <= m_tool_count; ++tool) {
        if (tool == fine_tool_1based)
            continue;
        const double diameter = nozzle_diameter(tool);
        if (diameter > coarse_nozzle || coarse_tool == fine_tool_1based) {
            coarse_tool = tool;
            coarse_nozzle = diameter;
        }
    }
    if (coarse_tool == fine_tool_1based && m_tool_count >= 2)
        coarse_tool = fine_tool_1based == 1 ? 2 : 1;
    return coarse_tool;
}

int MixedNozzleWorkstationDialog::selected_tool(wxChoice* choice, const bool allow_default) const
{
    if (choice == nullptr)
        return allow_default ? 0 : 1;
    const int selection = choice->GetSelection();
    if (allow_default && selection <= 0)
        return 0;
    return std::clamp(allow_default ? selection : selection + 1, 1, m_tool_count);
}

void MixedNozzleWorkstationDialog::select_tool(wxChoice* choice, const int tool_1based, const bool allow_default)
{
    if (choice == nullptr)
        return;
    if (allow_default && tool_1based <= 0) {
        choice->SetSelection(0);
        return;
    }
    const int clamped_tool = std::clamp(tool_1based, 1, m_tool_count);
    choice->SetSelection(allow_default ? clamped_tool : clamped_tool - 1);
}

wxString MixedNozzleWorkstationDialog::selected_tool_label(const int tool_1based, const int inherited_tool_1based) const
{
    if (tool_1based == 0) {
        const int inherited = std::clamp(inherited_tool_1based, 1, m_tool_count);
        return wxString::Format("%s -> T%d", _L("Default"), inherited - 1);
    }
    const int tool = std::clamp(tool_1based, 1, m_tool_count);
    return wxString::Format("T%d", tool - 1);
}

wxString MixedNozzleWorkstationDialog::tool_label(const int tool_1based) const
{
    const int tool = std::clamp(tool_1based, 1, m_tool_count);
    return wxString::Format("T%d  %.2f mm  ", tool - 1, nozzle_diameter(tool)) +
           wxString::FromUTF8(m_filament_names[tool - 1]);
}

wxString MixedNozzleWorkstationDialog::physical_tool_label(const int logical_tool_1based) const
{
    const int logical = std::clamp(logical_tool_1based, 1, m_tool_count) - 1;
    const int physical = logical >= 0 && logical < int(m_physical_tool_indices.size()) &&
                         m_physical_tool_indices[logical] >= 0 ? m_physical_tool_indices[logical] : logical;
    return wxString::Format(_L("Physical %d"), physical + 1);
}

double MixedNozzleWorkstationDialog::nozzle_diameter(const int tool_1based) const
{
    return m_nozzle_diameters[std::clamp(tool_1based, 1, m_tool_count) - 1];
}

double MixedNozzleWorkstationDialog::max_layer_height(const int tool_1based) const
{
    return m_max_layer_heights[std::clamp(tool_1based, 1, m_tool_count) - 1];
}

double MixedNozzleWorkstationDialog::suggested_line_width(const int tool_1based, const double multiplier) const
{
    return std::max(0.05, nozzle_diameter(tool_1based) * multiplier);
}

void MixedNozzleWorkstationDialog::set_feature_line_width(DynamicPrintConfig& print_config,
                                                          const char* key,
                                                          const int /*tool_1based*/,
                                                          const double multiplier) const
{
    print_config.set_key_value(key, new ConfigOptionFloatOrPercent(std::max(1., multiplier * 100.), true));
}

void MixedNozzleWorkstationDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    Fit();
    SetSize(suggested_rect.GetSize());
    Refresh();
}

}} // namespace Slic3r::GUI
