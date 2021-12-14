# unzip extract "preset_backup.zip" to "./"
# and the structure should look like this: ["./preset_backup/Acoustic","./preset_backup/Alternative", etc., ...]
# execute the script in "./"
# and it will rename and move presets to folder "./preset_backup/"


import os
import json

folderList = [folder.path for folder in os.scandir('./preset_backup') if folder.is_dir()]
print(f"folder list obtailed: \t {folderList}")

for i in folderList:
    with open(f"{i}/category.json", "r") as f:
        filelist_raw = f.read()
        filelist_dict = json.loads(filelist_raw)
        print(f"filename list of '{i}' obtained,")
        count = 0
        for j in filelist_dict["presets"]:
            folderName = j["id"]
            if not (os.path.exists(f"{i}/{folderName}/preset.json")):
                continue
            presetName = j["name"]
            if (os.path.exists(f"./preset_backup/{presetName}.json")):
                continue
            count+=1
            os.rename(f"{i}/{folderName}/preset.json",f"./preset_backup/{presetName}.json")
            print(f"{count}. \t{presetName}:\t./{folderName}\t...done.")

print("end of process")
