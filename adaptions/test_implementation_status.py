from mdutils.mdutils import MdUtils
import pytest

color_wheel = [
    # "#260",
    "#800",
    "#840",
    "#880",
    "#480",
    "#4a0",
    "#0ac",
    "#08a",
    "#048",
    "#008",
    "#440"]

column_names = ["Synth name",
                "numberOfBanks",
                "numberOfPatchesPerBank",
                "bankDescriptors",
                "createDeviceDetectMessage",
                "channelIfValidDeviceResponse",
                "needsChannelSpecificDetection",
                "deviceDetectWaitMilliseconds",
                "Edit Buffer Capability",
                "isPartOfEditBufferDump",
                "Program Dump Capability",
                "isPartOfSingleProgramDump",
                "Bank Dump Capability",
                "numberFromDump",
                "nameFromDump",
                "generalMessageDelay",
                "renamePatch",
                "isDefaultName",
                "calculateFingerprint",
                "friendlyBankName",
                "friendlyProgramName",
                "Layer Capability",
                "setLayerName",
                "setupHelp",
                ]
table_result = ["Synth name" if i == 0 else str(i + 1) for i in range(len(column_names))]
yes_char = "Y"
no_char = "-"
total_columns = 0

mdFile = MdUtils(file_name='implementation_overview.md', title='Adaptation: Implementation status')
mdFile.write("This table lists which implementation has implemented which function or capability.\n\n"
             "Note that not all synths need to implement all functions, so it is not necessarily an incomplete implementation"
             " if some columns are marked as not implemented\n\n")
legend_table = []


def write_colored_table(table_data, num_columns, colors_in_column):
    mdFile.write("<table>")
    colno = 0
    num_fields = len(table_data)
    for rowno in range(num_fields // num_columns):
        row_start = rowno * num_columns
        mdFile.write("<tr>")
        if not colors_in_column:
            # Rows are coloured
            if rowno == 0:
                color = "#260"
            else:
                color = color_wheel[colno]
                colno = (colno + 1) % len(color_wheel)
        for col in range(num_columns):
            if colors_in_column:
                if col == 0:
                    color = "#260"
                    colno = 0
                else:
                    color = color_wheel[colno]
                    colno = (colno + 1) % len(color_wheel)
            mdFile.write(f'<td text-align="center" style="color: #ffffff; background-color: {color}">{table_data[row_start + col]}</td>')
        mdFile.write("</tr>")
    mdFile.write("</table>")


@pytest.fixture(scope="session")
def md_file():
    global table_result
    global mdFile
    for i in range(len(column_names)):
        legend_table.extend([str(i + 1), column_names[i]])
    yield mdFile
    write_colored_table(table_result, total_columns, True)
    mdFile.write("\nColumn explanation:\n\n")
    write_colored_table(legend_table, 2, False)
    mdFile.create_md_file()


def check(adaptation, method_name):
    if hasattr(adaptation, method_name):
        return yes_char
    return no_char


def capability_check(adaptation, list_of_methods):
    if all(hasattr(adaptation, x) for x in list_of_methods):
        return yes_char
    return no_char


def test_implementation_status(adaptation, md_file):
    global table_result
    global total_columns
    my_row = [adaptation.name(),
              check(adaptation, "numberOfBanks"),
              check(adaptation, "numberOfPatchesPerBank"),
              check(adaptation, "bankDescriptors"),
              check(adaptation, "createDeviceDetectMessage"),
              check(adaptation, "channelIfValidDeviceResponse"),
              check(adaptation, "needsChannelSpecificDetection"),
              check(adaptation, "deviceDetectWaitMilliseconds"),
              capability_check(adaptation, ["createEditBufferRequest", "isEditBufferDump", "convertToEditBuffer"]),
              check(adaptation, "isPartOfEditBufferDump"),
              capability_check(adaptation, ["createProgramDumpRequest", "isSingleProgramDump", "convertToProgramDump"]),
              check(adaptation, "isPartOfSingleProgramDump"),
              capability_check(adaptation, ["createBankDumpRequest", "isPartOfBankDump", "isBankDumpFinished", "extractPatchesFromBank"]),
              check(adaptation, "numberFromDump"),
              check(adaptation, "nameFromDump"),
              check(adaptation, "generalMessageDelay"),
              check(adaptation, "renamePatch"),
              check(adaptation, "isDefaultName"),
              check(adaptation, "calculateFingerprint"),
              check(adaptation, "friendlyBankName"),
              check(adaptation, "friendlyProgramName"),
              capability_check(adaptation, ["numberOfLayers", "layerName"]),
              check(adaptation, "setLayerName"),
              check(adaptation, "setupHelp"),
              ]
    table_result.extend(my_row)
    total_columns = len(my_row)
