# delete /runs folder
import os
import shutil
shutil.rmtree("runs") if os.path.exists("runs") else None