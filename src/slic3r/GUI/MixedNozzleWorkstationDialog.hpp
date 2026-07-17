#pragma once

#include "GUI_Utils.hpp"

#include "libslic3r/PrintConfig.hpp"

#include <string>
#include <vector>

class wxCheckBox;
class wxChoice;
class wxNotebook;
class wxSpinCtrlDouble;
class wxStaticText;

namespace Slic3r {

class DynamicPrintConfig;

namespace GUI {

class MixedNozzleWorkstationDialog : public DPIDialog
{
public:
    MixedNozzleWorkstationDialog(wxWindow* parent,
                                 const DynamicPrintConfig& print_config,
                                 const DynamicPrintConfig& printer_config,
                                 const std::vector<std::string>& filament_names,
                                 const std::vector<int>& physical_tool_indices);

    void apply_to_config(DynamicPrintConfig& print_config) const;
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    enum class Strategy {
        SameLayer = 0,
        SparseOnly,
        SparseAndInnerWall,
        FullInternalExperimental
    };

    void build_ui();
    void load_from_config();
    void apply_strategy(Strategy strategy);
    void update_validation();
    void update_manual_height_state();

    int recommended_fine_tool() const;
    int recommended_coarse_tool(int fine_tool_1based) const;
    int selected_tool(wxChoice* choice, bool allow_default = false) const;
    void select_tool(wxChoice* choice, int tool_1based, bool allow_default = false);
    wxString selected_tool_label(int tool_1based, int inherited_tool_1based = 1) const;
    wxString tool_label(int tool_1based) const;
    wxString physical_tool_label(int logical_tool_1based) const;
    double nozzle_diameter(int tool_1based) const;
    double max_layer_height(int tool_1based) const;
    double suggested_line_width(int tool_1based, double multiplier) const;
    void set_feature_line_width(DynamicPrintConfig& print_config, const char* key, int tool_1based, double multiplier) const;

    const DynamicPrintConfig& m_print_config;
    const DynamicPrintConfig& m_printer_config;
    std::vector<std::string>  m_filament_names;
    std::vector<double>       m_nozzle_diameters;
    std::vector<double>       m_max_layer_heights;
    std::vector<int>          m_physical_tool_indices;
    int                       m_tool_count = 1;

    wxChoice*         m_strategy_choice = nullptr;
    wxChoice*         m_outer_tool = nullptr;
    wxChoice*         m_inner_wall_tool = nullptr;
    wxChoice*         m_sparse_infill_tool = nullptr;
    wxChoice*         m_solid_infill_tool = nullptr;
    wxCheckBox*       m_combine_sparse_infill = nullptr;
    wxCheckBox*       m_combine_inner_wall = nullptr;
    wxCheckBox*       m_combine_internal_solid = nullptr;
    wxCheckBox*       m_auto_coarse_layer_height = nullptr;
    wxSpinCtrlDouble* m_coarse_layer_height = nullptr;
    wxStaticText*     m_validation_text = nullptr;
};

}} // namespace Slic3r::GUI
