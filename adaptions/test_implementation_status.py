from mdutils.mdutils import MdUtils
import pytest

column_names = ["Synth name",
                "numberOfBanks",
                "numberOfPatchesPerBank",
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
yes_char = "X"
no_char = "-"
num_columns = 0

mdFile = MdUtils(file_name='implementation_overview.md', title='Adaptation: Implementation status')
mdFile.write("This table lists which implementation has implemented which function or capability.\n\n"
             "Note that not all synths need to implement all functions, so it is not necessarily an incomplete implementation"
             " if some columns are marked as not implemented\n\n")
legend_table = []


@pytest.fixture(scope="session")
def md_file():
    global table_result
    global mdFile
    for i in range(len(column_names)):
        legend_table.extend([str(i + 1), column_names[i]])
    yield mdFile
    num_fields = len(table_result)
    mdFile.new_table(columns=num_columns, rows=num_fields // num_columns, text=table_result, text_align='center')
    mdFile.write("\nColumn explanation:\n\n")
    mdFile.new_table(columns=2, rows=len(legend_table) // 2, text=legend_table, text_align='center')
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
    global num_columns
    my_row = [adaptation.name(),
              check(adaptation, "numberOfBanks"),
              check(adaptation, "numberOfPatchesPerBank"),
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
    num_columns = len(my_row)
