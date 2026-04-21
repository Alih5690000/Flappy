gh workflow run build.yml --ref main && gh run watch && cd dist && rm * && gh run download && python -m http.server 8000
#eval "$(ssh-agent -s)"
#ssh-add ~/.ssh/id_rsa
