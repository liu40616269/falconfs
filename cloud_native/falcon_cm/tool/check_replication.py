from kazoo.client import KazooClient
import psycopg2
import os


def get_zk_nodes(zk_hosts, path):
    zk = KazooClient(hosts=zk_hosts)
    zk.start()

    nodes = {}
    if zk.exists(path):
        children = zk.get_children(path)
        for child in children:
            ip_port, _ = zk.get("{}/{}".format(path, child))
            ip = ip_port.decode('utf-8').split(':', 1)[0]
            nodes[child] = ip

    zk.stop()
    return nodes

def get_db_servers(db_host, db_port, db_name, user):
    conn = psycopg2.connect(host=db_host, port=db_port, dbname=db_name, user=user)
    cursor = conn.cursor()
    cursor.execute("SELECT server_name,host FROM falcon_foreign_server;")
    db_nodes = {str(row[0]):row[1] for row in cursor.fetchall()}

    cursor.close()
    conn.close()
    return db_nodes

def compare_nodes(zk_nodes, db_nodes):
    zk_set = set(zk_nodes.items())
    db_set = set(db_nodes.items())

    if zk_set == db_set:
        return True
    else:
        return False

def check_replication(ip, db_port, db_name, user):
    conn = psycopg2.connect(host=ip, port=db_port, dbname=db_name, user=user)
    cursor = conn.cursor()
    cursor.execute("SELECT client_addr,state,sync_state FROM pg_stat_replication;")
    replication_info = cursor.fetchall()

    cursor.close()
    conn.close()
    return replication_info

if __name__ == '__main__':
    zk_endpoint = os.getenv("zk_endpoint")
    cluster_name = os.environ.get("cluster_name", "/falcon")
    nodes_path = "{}/leaders".format(cluster_name)
    db_port = int(os.environ.get("meta_port", "5432"))
    db_name = "postgres"
    user = os.environ.get("user_name", "falconMeta")

    zk_nodes = get_zk_nodes(zk_endpoint, nodes_path)
    db_host = zk_nodes["cn"]
    db_nodes = get_db_servers(db_host, db_port, db_name, user)
    if compare_nodes(zk_nodes, db_nodes):
        for node, ip in zk_nodes.items():
            replication_info = check_replication(ip, db_port, db_name, user)
            if len(replication_info) == 2 and all(state == 'streaming' for _, state, _ in replication_info):
                print("node {} is OK!".format(node))
            else:
                print("node {} {} is not OK".format(node, ip))
                print("replication info is: {}".format(replication_info))
                replicas_path = "{}/falcon_clusters/{}/replicas".format(cluster_name, node)
                replicas_info = get_zk_nodes(zk_endpoint, replicas_path)
                print(f"replicas info in zk : {replicas_info}")
    else:
        print("nodes is not the same")