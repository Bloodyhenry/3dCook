# Passerelle recettes → 3DS
# Image légère : Python + Pillow, aucune autre dépendance.
FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    RECETTES_SOURCE=/source \
    RECETTES_DATA=/data \
    RECETTES_PORT=8383

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY recettes3ds ./recettes3ds

EXPOSE 8383
# Le NAS redémarre le conteneur si la passerelle ne répond plus.
HEALTHCHECK --interval=60s --timeout=5s --start-period=20s \
  CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8383/menu', timeout=4)" || exit 1

CMD ["python", "-m", "recettes3ds"]
